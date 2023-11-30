// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/visual_query/visual_query_classifier_host.h"

#include "base/base64.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/companion/core/companion_metrics_logger.h"
#include "chrome/browser/companion/visual_query/visual_query_suggestions_service.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/encode/SkJpegEncoder.h"
#include "ui/gfx/image/buffer_w_stream.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace companion::visual_query {

namespace {

void RecordStatusChange(InitStatus status) {
  base::UmaHistogramEnumeration(
      "Companion.VisualQuery.ClassificationInitStatus", status);
}

absl::optional<std::string> Base64EncodeBitmap(const SkBitmap& bitmap) {
  gfx::BufferWStream stream;
  const bool encoding_succeeded =
      SkJpegEncoder::Encode(&stream, bitmap.pixmap(), {});

  if (!encoding_succeeded) {
    return absl::nullopt;
  }

  base::StringPiece mime_subtype = "jpg";
  std::string result = "data:image/";
  result.append(mime_subtype.begin(), mime_subtype.end());
  result.append(";base64,");
  result.append(
      base::Base64Encode(base::as_bytes(base::make_span(stream.TakeBuffer()))));
  return result;
}

// Close the provided model file.
void CloseModelFile(base::File model_file) {
  if (!model_file.IsValid()) {
    return;
  }
  model_file.Close();
}

// Convert metrics map from Mojom IPC to |VisualSuggestionMetrics|.
VisualSuggestionsMetrics GenerateMetrics(const ClassificationStats& stats) {
  VisualSuggestionsMetrics metrics;
  metrics.eligible_count = stats->eligible_count;
  metrics.shoppy_count = stats->shoppy_count;
  metrics.sensitive_count = stats->sensitive_count;
  metrics.shoppy_nonsensitive_count = stats->shoppy_nonsensitive_count;
  metrics.results_count = stats->results_count;
  return metrics;
}

}  // namespace

VisualQueryClassifierHost::VisualQueryClassifierHost(
    VisualQuerySuggestionsService* visual_query_service)
    : visual_query_service_(visual_query_service),
      current_result_(VisualQueryResultPair()) {}

VisualQueryClassifierHost::~VisualQueryClassifierHost() = default;

void VisualQueryClassifierHost::HandleClassification(
    std::vector<mojom::VisualQuerySuggestionPtr> results,
    mojom::ClassificationStatsPtr classification_stats) {
  base::UmaHistogramCounts100("Companion.VisualQuery.ClassificationResultsSize",
                              results.size());
  VisualSuggestionsResults converted_results;
  converted_results.reserve(results.size());

  // converts list of SkBitmaps to data uris used as img.src for browser.
  for (const auto& result : results) {
    auto data_uri = Base64EncodeBitmap(result->image);
    if (data_uri) {
      VisualSuggestionsResult converted = {data_uri.value(), result->alt_text};
      converted_results.emplace_back(std::move(converted));
    }
  }

  // We cache a copy of the result for the current URL to avoid reprocessing.
  current_result_->second = converted_results;
  waiting_for_result_ = false;

  LOCAL_HISTOGRAM_BOOLEAN("Companion.VisualQuery.EndClassificationSuccess",
                          !result_callback_.is_null());
  if (!result_callback_.is_null()) {
    std::move(result_callback_)
        .Run(std::move(converted_results),
             GenerateMetrics(classification_stats));
  }
  // Log latency from the time the companion page handler called
  // StartClassification to now, after the classification results have been
  // passed to the callback.
  base::UmaHistogramTimes("Companion.VisualQuery.ClassificationLatency",
                          base::TimeTicks::Now() - classification_start_time_);
  // Reset the start time tracker. It will be set again the next time
  // StartClassification is run.
  classification_start_time_ = base::TimeTicks();
  result_callback_.Reset();
  result_handler_.reset();
}

void VisualQueryClassifierHost::StartClassification(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    ResultCallback callback) {
  // Start time is used to compute duration of classifier init and of
  // the overall classification.
  classification_start_time_ = base::TimeTicks::Now();
  if (!render_frame_host) {
    RecordStatusChange(InitStatus::kEmptyRenderFrame);
    return;
  }

  // We check to see if callback is already set signifying an ongoing query.
  if (!result_callback_.is_null()) {
    RecordStatusChange(InitStatus::kOngoingClassification);
    return;
  }

  // We store the current url being processed in the last result pair.
  current_result_->first = validated_url;

  // We set the callback so that we know where to send back the results.
  result_callback_ = std::move(callback);

  // Use |render_frame_host| to get visual query mojom IPC running in renderer.
  mojo::AssociatedRemote<mojom::VisualSuggestionsRequestHandler> visual_query;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &visual_query);

  visual_query_service_->RegisterModelUpdateCallback(
      base::BindOnce(&VisualQueryClassifierHost::StartClassificationWithModel,
                     weak_ptr_factory_.GetWeakPtr(), std::move(visual_query)));

  base::UmaHistogramEnumeration(
      "Companion.VisualQuery.ClassificationInitStatus",
      InitStatus::kFetchModel);
}

void VisualQueryClassifierHost::StartClassificationWithModel(
    mojo::AssociatedRemote<mojom::VisualSuggestionsRequestHandler>
        visual_query,
    base::File model,
    const std::string& base64_config) {
  base::UmaHistogramBoolean("Companion.VisualQuery.ClassifierModelAvailable",
                            model.IsValid());
  if (!model.IsValid()) {
    RecordStatusChange(InitStatus::kModelUnavailable);
    return;
  }

  if (result_callback_.is_null()) {
    RecordStatusChange(InitStatus::kCallbackCancelled);
    return;
  }

  if (visual_query.is_bound() && !result_handler_.is_bound()) {
    visual_query->StartVisualClassification(
        std::move(model), base64_config,
        result_handler_.BindNewPipeAndPassRemote());

    // Keep track that we sent IPC and waiting on renderer.
    waiting_for_result_ = true;

    // Log latency from the time the companion page handler called
    // StartClassification to now, after the proper checks and sets have
    // completed and classification has actually started.
    base::UmaHistogramTimes(
        "Companion.VisualQuery.ClassifierInitializationLatency",
        base::TimeTicks::Now() - classification_start_time_);
    RecordStatusChange(InitStatus::kSuccess);
  } else {
    // Closing file in background thread since we did not make IPC.
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&CloseModelFile, std::move(model)));
    RecordStatusChange(InitStatus::kIpcNotMade);
  }
}

void VisualQueryClassifierHost::CancelClassification(const GURL& visible_url) {
  result_callback_.Reset();
  waiting_for_result_ = false;
  RecordStatusChange(InitStatus::kQueryCancelled);
}

absl::optional<VisualSuggestionsResults>
VisualQueryClassifierHost::GetVisualResult(const GURL& url) {
  // We only send back results if we have received result from the renderer.
  if (!waiting_for_result_ && current_result_ &&
      url.GetContent() == current_result_->first.GetContent()) {
    return absl::make_optional(current_result_->second);
  }
  return absl::nullopt;
}
}  // namespace companion::visual_query
