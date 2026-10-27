// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/selection/suggestion_service.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/selection/features.h"
#include "components/optimization_guide/core/feature_registry/feature_keys.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "components/variations/service/google_groups_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "skia/ext/image_operations.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace selection {

namespace {

namespace proto = ::optimization_guide::proto;

// If `kSmartSelectionServerCropPadding` is non-negative, crops `screenshot` to
// `selection` expanded by the padding and updates `selection` to be relative to
// the top-left of the cropped screenshot (as required by
// `proto::AreaOfInterest::selection`).
void MaybeCropScreenshot(SkBitmap& screenshot, gfx::Rect& selection) {
  const int padding = kSmartSelectionServerCropPadding.Get();
  if (padding < 0 || screenshot.drawsNothing()) {
    return;
  }
  const gfx::Rect screenshot_bounds = gfx::SkIRectToRect(screenshot.bounds());
  const gfx::Rect clamped_selection =
      gfx::IntersectRects(selection, screenshot_bounds);
  if (clamped_selection.IsEmpty()) {
    return;
  }
  gfx::Rect padded_selection = clamped_selection;
  padded_selection.Outset(padding);
  padded_selection.Intersect(screenshot_bounds);
  SkBitmap cropped;
  if (!screenshot.extractSubset(&cropped,
                                gfx::RectToSkIRect(padded_selection))) {
    return;
  }
  // Shift `selection` so its origin is relative to the top-left of `cropped`.
  selection = clamped_selection - padded_selection.OffsetFromOrigin();
  screenshot = std::move(cropped);
}

// If `kSmartSelectionServerMaxScreenshotDimension` is non-negative and
// `screenshot` exceeds this size, downscales `screenshot` so that its longest
// side is at most this value, and scales `selection` accordingly.
void MaybeDownscaleScreenshot(SkBitmap& screenshot, gfx::Rect& selection) {
  const int max_dimension = kSmartSelectionServerMaxScreenshotDimension.Get();
  if (max_dimension < 0 || screenshot.drawsNothing()) {
    return;
  }
  const int longest_side = std::max(screenshot.width(), screenshot.height());
  if (longest_side <= max_dimension) {
    return;  // Prevent upscaling.
  }
  if (max_dimension == 0) {
    screenshot.reset();
    selection = gfx::Rect();
    return;
  }

  const float scale = static_cast<float>(max_dimension) / longest_side;
  SkBitmap downscaled = skia::ImageOperations::Resize(
      screenshot, skia::ImageOperations::RESIZE_BEST,
      std::max(1, base::ClampRound(scale * screenshot.width())),
      std::max(1, base::ClampRound(scale * screenshot.height())));
  if (downscaled.drawsNothing()) {
    return;
  }
  selection = gfx::ScaleToEnclosingRect(selection, scale);
  screenshot = std::move(downscaled);
}

struct AreaOfInterestWithScreenshot {
  proto::AreaOfInterest proto_aoi;
  SkBitmap screenshot;
};

// Converts `aoi` into its proto representation (excluding screenshot data) and
// returns the (potentially cropped) screenshot to encode.
AreaOfInterestWithScreenshot ToProtoAreaOfInterest(const AreaOfInterest& aoi) {
  SkBitmap screenshot = aoi.screenshot;
  proto::AreaOfInterest proto_aoi;
  std::visit(absl::Overload{
                 [&](gfx::Rect selection_rect) {
                   MaybeCropScreenshot(screenshot, selection_rect);
                   MaybeDownscaleScreenshot(screenshot, selection_rect);
                   proto::Rectangle& selection = *proto_aoi.mutable_selection();
                   selection.set_x(selection_rect.x());
                   selection.set_y(selection_rect.y());
                   selection.set_width(selection_rect.width());
                   selection.set_height(selection_rect.height());
                 },
                 [](base::span<const gfx::Point> coordinates) {},
             },
             aoi.bounds);
  if (kSmartSelectionServerAppendApc.Get()) {
    *proto_aoi.mutable_annotated_page_content() = aoi.apc;
  }
  return {std::move(proto_aoi), std::move(screenshot)};
}

struct SuggestionRequestWithScreenshot {
  proto::SmartSelectionSuggestionsRequest request;
  SkBitmap screenshot;
};

// Builds a request proto for `aoi` and `tools` (excluding screenshot data) and
// returns the (potentially cropped) screenshot to encode.
SuggestionRequestWithScreenshot BuildServerSuggestionsRequest(
    const AreaOfInterest& aoi,
    const base::flat_map<SuggestionTool::ToolId, raw_ptr<SuggestionTool>>&
        tools) {
  proto::SmartSelectionSuggestionsRequest request;
  proto::SmartSelectionClientCapabilities& capabilities =
      *request.mutable_client_capabilities();
  for (const auto& [tool_id, tool] : tools) {
    proto::SmartSelectionToolWithCapabilities& tool_cap =
        *capabilities.add_available_tools();
    tool_cap.set_tool(tool_id);
    std::vector<std::string> tool_capabilities = tool->GetCapabilities();
    *tool_cap.mutable_capabilities() = {
        std::make_move_iterator(tool_capabilities.begin()),
        std::make_move_iterator(tool_capabilities.end())};
  }

  if (g_browser_process) {
    capabilities.set_locale(g_browser_process->GetApplicationLocale());
  }
  capabilities.set_platform(optimization_guide::GetChromePlatform());
  capabilities.set_chrome_version(
      std::string(version_info::GetVersionNumber()));

  auto [proto_aoi, screenshot] = ToProtoAreaOfInterest(aoi);
  *request.add_areas_of_interest() = std::move(proto_aoi);
  return {std::move(request), std::move(screenshot)};
}

// Invokes the corresponding `tools` to create `Suggestion`s from `response`.
std::vector<std::unique_ptr<Suggestion>> ExtractSuggestionsFromResponse(
    const AreaOfInterest& aoi,
    const proto::SmartSelectionSuggestionsResponse& response,
    const base::flat_map<SuggestionTool::ToolId, raw_ptr<SuggestionTool>>&
        tools) {
  std::vector<std::unique_ptr<Suggestion>> suggestions;
  suggestions.reserve(response.suggestions().size());
  for (const proto::SmartSelectionSuggestion& server_suggestion :
       response.suggestions()) {
    if (const raw_ptr<SuggestionTool>* tool =
            base::FindOrNull(tools, server_suggestion.tool())) {
      if (std::unique_ptr<Suggestion> suggestion =
              (*tool)->CreateSuggestion(aoi, server_suggestion)) {
        suggestions.emplace_back(std::move(suggestion));
      }
    }
  }
  return suggestions;
}

void RecordLatencyHistogram(std::string_view stage, base::TimeDelta duration) {
  // Uses a custom 1 ms to 20 s range because
  // - UmaHistogramTimes() caps at 10 s, which is below the server timeout;
  // - UmaHistogramMediumTimes() starts at 10 ms, which is above the typical
  //   latency for the fast sub-stages.
  base::UmaHistogramCustomTimes(
      base::StrCat({"SmartSelection.Suggestions.Latency.", stage}),
      std::max(base::TimeDelta(), duration),
      /*min=*/base::Milliseconds(1),
      /*max=*/base::Seconds(20),
      /*buckets=*/50);
}

void LogLatencyMetrics(content::WebContents* web_contents,
                       base::TimeTicks request_start_time,
                       base::TimeTicks mes_request_start_time,
                       size_t num_image_bytes,
                       size_t num_apc_bytes,
                       const proto::SmartSelectionLatencyMetrics& metrics,
                       size_t num_suggestions) {
  const base::TimeTicks now = base::TimeTicks::Now();
  struct {
    base::TimeDelta end_to_end;
    base::TimeDelta client_mes_rpc;
    base::TimeDelta client_prep;
    base::TimeDelta transmission;
    base::TimeDelta mes_total;
    base::TimeDelta mes_overhead;
    base::TimeDelta mes_image_prep;
    base::TimeDelta mes_other_overhead;
    base::TimeDelta model_run;
    base::TimeDelta model_content_proc;
    base::TimeDelta model_prefill_queue_wait;
    base::TimeDelta model_prefill;
    base::TimeDelta model_decode_queue_wait;
    base::TimeDelta model_decode;
    base::TimeDelta model_other;
  } durations = {
      .end_to_end = now - request_start_time,
      .client_mes_rpc = now - mes_request_start_time,
      .client_prep = mes_request_start_time - request_start_time,
  };

  if (metrics.has_mes_total_ms()) {
    durations.mes_total = base::Milliseconds(metrics.mes_total_ms());
    durations.transmission = durations.client_mes_rpc - durations.mes_total;
    if (metrics.has_model_execution_ms()) {
      durations.model_run = base::Milliseconds(metrics.model_execution_ms());
      durations.mes_overhead = durations.mes_total - durations.model_run;
      if (metrics.has_mes_image_preprocessing_ms()) {
        durations.mes_image_prep =
            base::Milliseconds(metrics.mes_image_preprocessing_ms());
        durations.mes_other_overhead =
            durations.mes_overhead - durations.mes_image_prep;
      }
      if (metrics.has_model_content_processing_ms() &&
          metrics.has_model_prefill_queue_wait_ms() &&
          metrics.has_model_prefill_ms() &&
          metrics.has_model_decode_queue_wait_ms() &&
          metrics.has_model_decode_ms()) {
        durations.model_content_proc =
            base::Milliseconds(metrics.model_content_processing_ms());
        durations.model_prefill_queue_wait =
            base::Milliseconds(metrics.model_prefill_queue_wait_ms());
        durations.model_prefill =
            base::Milliseconds(metrics.model_prefill_ms());
        durations.model_decode_queue_wait =
            base::Milliseconds(metrics.model_decode_queue_wait_ms());
        durations.model_decode = base::Milliseconds(metrics.model_decode_ms());
        durations.model_other =
            durations.model_run - durations.model_content_proc -
            durations.model_prefill_queue_wait - durations.model_prefill -
            durations.model_decode_queue_wait - durations.model_decode;
      }
    }
  }

  RecordLatencyHistogram("EndToEnd", durations.end_to_end);
  RecordLatencyHistogram("ClientPrep", durations.client_prep);
  RecordLatencyHistogram("ClientMesRpc", durations.client_mes_rpc);
  base::UmaHistogramCounts10000("SmartSelection.Suggestions.Request.ImageSize",
                                num_image_bytes / 1000);
  base::UmaHistogramCounts10000("SmartSelection.Suggestions.Request.ApcSize",
                                num_apc_bytes / 1000);
  base::UmaHistogramExactLinear(
      "SmartSelection.Suggestions.Response.SuggestionCount", num_suggestions,
      /*exclusive_max=*/10);

  if (metrics.has_mes_total_ms()) {
    RecordLatencyHistogram("MesTotal", durations.mes_total);
    RecordLatencyHistogram("Transmission", durations.transmission);
    if (metrics.has_model_execution_ms()) {
      RecordLatencyHistogram("ModelRun", durations.model_run);
      RecordLatencyHistogram("MesOverhead", durations.mes_overhead);
      if (metrics.has_mes_image_preprocessing_ms()) {
        RecordLatencyHistogram("MesImagePrep", durations.mes_image_prep);
        RecordLatencyHistogram("MesOtherOverhead",
                               durations.mes_other_overhead);
      }
      if (metrics.has_model_content_processing_ms() &&
          metrics.has_model_prefill_queue_wait_ms() &&
          metrics.has_model_prefill_ms() &&
          metrics.has_model_decode_queue_wait_ms() &&
          metrics.has_model_decode_ms()) {
        RecordLatencyHistogram("ModelContentProc",
                               durations.model_content_proc);
        RecordLatencyHistogram("ModelPrefillQueueWait",
                               durations.model_prefill_queue_wait);
        RecordLatencyHistogram("ModelPrefill", durations.model_prefill);
        RecordLatencyHistogram("ModelDecodeQueueWait",
                               durations.model_decode_queue_wait);
        RecordLatencyHistogram("ModelDecode", durations.model_decode);
        RecordLatencyHistogram("ModelOther", durations.model_other);
      }
    }
  }

  if (web_contents && web_contents->GetPrimaryMainFrame()) {
    const ukm::SourceId source_id =
        web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
    if (source_id != ukm::kInvalidSourceId) {
      auto as_bucket = [](base::TimeDelta duration) {
        return ukm::GetExponentialBucketMinForFineUserTiming(
            std::max(int64_t{0}, duration.InMilliseconds()));
      };

      ukm::builders::SmartSelection_Suggestions builder(source_id);
      builder.SetLatency_EndToEndMs(as_bucket(durations.end_to_end))
          .SetLatency_ClientPrepMs(as_bucket(durations.client_prep))
          .SetLatency_ClientMesRpcMs(as_bucket(durations.client_mes_rpc))
          .SetRequest_ImageSize(
              ukm::GetExponentialBucketMinForBytes(num_image_bytes))
          .SetRequest_ApcSize(
              ukm::GetExponentialBucketMinForBytes(num_apc_bytes))
          .SetResponse_SuggestionCount(
              std::min(static_cast<int64_t>(num_suggestions), int64_t{10}));

      if (metrics.has_mes_total_ms()) {
        builder.SetLatency_MesTotalMs(as_bucket(durations.mes_total))
            .SetLatency_TransmissionMs(as_bucket(durations.transmission));
        if (metrics.has_model_execution_ms()) {
          builder.SetLatency_ModelRunMs(as_bucket(durations.model_run))
              .SetLatency_MesOverheadMs(as_bucket(durations.mes_overhead));
          if (metrics.has_mes_image_preprocessing_ms()) {
            builder
                .SetLatency_MesImagePrepMs(as_bucket(durations.mes_image_prep))
                .SetLatency_MesOtherOverheadMs(
                    as_bucket(durations.mes_other_overhead));
          }
          if (metrics.has_model_content_processing_ms() &&
              metrics.has_model_prefill_queue_wait_ms() &&
              metrics.has_model_prefill_ms() && metrics.has_model_decode_ms() &&
              metrics.has_model_decode_queue_wait_ms()) {
            builder
                .SetLatency_ModelContentProcMs(
                    as_bucket(durations.model_content_proc))
                .SetLatency_ModelPrefillQueueWaitMs(
                    as_bucket(durations.model_prefill_queue_wait))
                .SetLatency_ModelPrefillMs(as_bucket(durations.model_prefill))
                .SetLatency_ModelDecodeMs(as_bucket(durations.model_decode))
                .SetLatency_ModelOtherMs(as_bucket(durations.model_other))
                .SetLatency_ModelDecodeQueueWaitMs(
                    as_bucket(durations.model_decode_queue_wait));
          }
        }
      }

      builder.Record(ukm::UkmRecorder::Get());
    }
  }

  VLOG(1) << "Suggestion latency: end-to-end: " << durations.end_to_end
          << " (client prep: " << durations.client_prep
          << ", transmission: " << durations.transmission
          << " [image: " << std::roundf(num_image_bytes / 1000.0)
          << " KB, apc: " << std::roundf(num_apc_bytes / 1000.0) << " KB]"
          << ", MES overhead: " << durations.mes_overhead
          << " [image prep: " << durations.mes_image_prep
          << ", other: " << durations.mes_other_overhead << "]"
          << ", model run: " << durations.model_run
          << " [content proc: " << durations.model_content_proc
          << ", prefill queue wait: " << durations.model_prefill_queue_wait
          << ", prefill: " << durations.model_prefill
          << ", decode queue wait: " << durations.model_decode_queue_wait
          << ", decode: " << durations.model_decode
          << ", other: " << durations.model_other << "]"
          << ") (suggestions=" << num_suggestions << ")";
}

}  // namespace

DEFINE_USER_DATA(SuggestionService);

struct SuggestionService::EncodedImage {
  std::vector<uint8_t> bytes;
  std::string mime_type;
};

// static
SuggestionService* SuggestionService::From(tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

// static
SuggestionService* SuggestionService::FromTabWebContents(
    content::WebContents* tab_web_contents) {
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(tab_web_contents);
  return From(tab);
}

SuggestionService::SuggestionService(
    tabs::TabInterface* tab,
    optimization_guide::RemoteModelExecutor* remote_model_executor,
    const GoogleGroupsManager* google_groups_manager)
    : tab_(CHECK_DEREF(tab)),
      remote_model_executor_(remote_model_executor),
      google_groups_manager_(google_groups_manager),
      scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this) {}

SuggestionService::~SuggestionService() {
  tools_.clear();
}

void SuggestionService::RegisterTool(SuggestionTool* tool) {
  if (!tool) {
    return;
  }
  const SuggestionTool::ToolId tool_id = tool->GetToolId();
  CHECK_NE(tool_id, SuggestionTool::ToolId::SMART_SELECTION_TOOL_UNSPECIFIED);
  CHECK(tools_.try_emplace(tool_id, tool).second);
}

void SuggestionService::UnregisterTool(SuggestionTool* tool) {
  if (!tool) {
    return;
  }
  if (auto it = tools_.find(tool->GetToolId());
      it != tools_.end() && it->second == tool) {
    tools_.erase(it);
  }
}

void SuggestionService::UpdateScreenContent(
    const SkBitmap& screenshot,
    const optimization_guide::proto::AnnotatedPageContent& apc) {}

void SuggestionService::RequestSuggestions(AreaOfInterest processed_area,
                                           SuggestionsCallback callback) {
  const auto request_start_time = base::TimeTicks::Now();
  if (tools_.empty() || !remote_model_executor_ ||
      !IsFeatureGroupEnabled(kSmartSelectionServerSuggestions)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  std::vector<std::unique_ptr<Suggestion>>()));
    return;
  }

  auto [request, screenshot] =
      BuildServerSuggestionsRequest(processed_area, tools_);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&EncodeScreenshot, std::move(screenshot)),
      base::BindOnce(&SuggestionService::SendServerSuggestionsRequest,
                     weak_factory_.GetWeakPtr(), std::move(processed_area),
                     std::move(callback), request_start_time,
                     std::move(request)));
}

bool SuggestionService::IsFeatureGroupEnabled(
    const base::Feature& feature) const {
  return google_groups_manager_
             ? google_groups_manager_->IsFeatureEnabledForProfile(feature)
             : base::FeatureList::IsEnabled(feature);
}

// static
std::optional<SuggestionService::EncodedImage>
SuggestionService::EncodeScreenshot(const SkBitmap& screenshot) {
  if (const int jpeg_quality = kSmartSelectionServerJpegQuality.Get();
      0 <= jpeg_quality && jpeg_quality <= 100) {
    return gfx::JPEGCodec::Encode(screenshot, jpeg_quality)
        .transform([](std::vector<uint8_t> bytes) {
          return EncodedImage{std::move(bytes), "image/jpeg"};
        });
  }
  return gfx::PNGCodec::EncodeBGRASkBitmap(screenshot,
                                           /*discard_transparency=*/false)
      .transform([](std::vector<uint8_t> bytes) {
        return EncodedImage{std::move(bytes), "image/png"};
      });
}

void SuggestionService::SendServerSuggestionsRequest(
    AreaOfInterest processed_area,
    SuggestionsCallback callback,
    base::TimeTicks request_start_time,
    proto::SmartSelectionSuggestionsRequest request,
    std::optional<EncodedImage> image) {
  if (image.has_value()) {
    CHECK_EQ(request.areas_of_interest_size(), 1);
    proto::AreaOfInterest& proto_aoi = *request.mutable_areas_of_interest(0);
    proto_aoi.set_image_bytes(base::as_string_view(image->bytes));
    proto_aoi.set_mime_type(image->mime_type);
  }

  const size_t num_image_bytes =
      request.areas_of_interest().empty()
          ? 0
          : request.areas_of_interest(0).image_bytes().size();
  const size_t num_apc_bytes = request.areas_of_interest().empty()
                                   ? 0
                                   : request.areas_of_interest(0)
                                         .annotated_page_content()
                                         .ByteSizeLong();

  // TODO(crbug.com/561489586): Consider adding logging.
  remote_model_executor_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kSmartSelectionSuggestions,
      request, {.execution_timeout = kSmartSelectionServerTimeout.Get()},
      base::BindOnce(&SuggestionService::OnServerSuggestions,
                     weak_factory_.GetWeakPtr(), std::move(processed_area),
                     std::move(callback), request_start_time,
                     /*mes_request_start_time=*/base::TimeTicks::Now(),
                     num_image_bytes, num_apc_bytes));
}

void SuggestionService::OnServerSuggestions(
    AreaOfInterest processed_area,
    SuggestionsCallback callback,
    base::TimeTicks request_start_time,
    base::TimeTicks mes_request_start_time,
    size_t num_image_bytes,
    size_t num_apc_bytes,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  std::optional<proto::SmartSelectionSuggestionsResponse> response;
  if (result.response.has_value()) {
    response = optimization_guide::ParsedAnyMetadata<
        proto::SmartSelectionSuggestionsResponse>(result.response.value());
  }
  if (!response) {
    std::move(callback).Run({});
    return;
  }
  std::vector<std::unique_ptr<Suggestion>> suggestions =
      ExtractSuggestionsFromResponse(processed_area, *response, tools_);
  LogLatencyMetrics(tab_->GetContents(), request_start_time,
                    mes_request_start_time, num_image_bytes, num_apc_bytes,
                    response->latency_metrics(), suggestions.size());
  std::move(callback).Run(std::move(suggestions));
}

}  // namespace selection
