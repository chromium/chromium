// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/get_pointer_tool.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/get_pointer_metrics.h"
#include "chrome/browser/ttc/core/get_pointer_response.h"
#include "chrome/browser/ttc/core/page_context_util.h"
#include "chrome/browser/ttc/core/session_controller_impl.h"
#include "chrome/browser/ttc/core/session_journal_impl.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/point.h"
#include "url/gurl.h"

namespace ttc {

namespace {

// Returns the points that get_pointer's `response` tells the model about.
std::vector<gfx::Point> GetPointsInResponse(const ToolResponse& response) {
  std::vector<gfx::Point> points;
  const base::ListValue* list =
      response.Ok() ? response.GetResult().FindList("points") : nullptr;
  if (!list) {
    return points;
  }
  for (const base::Value& value : *list) {
    const base::DictValue& point = value.GetDict();
    const std::optional<int> x = point.FindInt("x");
    const std::optional<int> y = point.FindInt("y");
    if (x && y) {
      points.emplace_back(*x, *y);
    }
  }
  return points;
}

GetPointerOutcome ToGetPointerOutcome(const ToolResponse& response) {
  if (response.Ok()) {
    // A response without an image says that the user is not pointing.
    return response.visual_content() ? GetPointerOutcome::kOk
                                     : GetPointerOutcome::kNoActivePointer;
  }
  // An error that is not one of the actor's has no code.
  if (!response.error().code) {
    return GetPointerOutcome::kOtherError;
  }
  switch (*response.error().code) {
    case actor::mojom::ActionResultCode::kUrlBlocked:
    case actor::mojom::ActionResultCode::kScreenshotCapturePageNotEligible:
      return GetPointerOutcome::kPageNotAllowed;
    case actor::mojom::ActionResultCode::kScreenshotCapturePageChanged:
      return GetPointerOutcome::kPageChanged;
    case actor::mojom::ActionResultCode::kToolTimeout:
      return GetPointerOutcome::kTimedOut;
    case actor::mojom::ActionResultCode::kScreenshotCaptureFailed:
      return GetPointerOutcome::kCaptureFailed;
    default:
      return GetPointerOutcome::kOtherError;
  }
}

}  // namespace

GetPointerTool::GetPointerTool(SessionControllerImpl& session_controller)
    : session_controller_(session_controller) {
  CHECK(base::FeatureList::IsEnabled(kTtcGetPointer));
}

GetPointerTool::~GetPointerTool() = default;

// static
ToolDefinition GetPointerTool::GetDefinition() {
  ToolDefinition definition;
  definition.name = "get_pointer";
  definition.description = base::StrCat(
      {"Returns a small image of the active webpage around where the user's "
       "pointer has been during their current request, with a crosshair "
       "marking the pointed-at spot, plus the page element under it. Use it "
       "when the user refers to something on screen with words like "
       "\"this\", \"that\", \"here\" or \"there\" without naming it. ",
       kTtcGetPointerIncludeNodeIdentity.Get()
           ? ""
           : "The element comes without a node id; to act on it, find it in "
             "the page content by its label or text. ",
       "If it returns no_active_pointer, the user is not pointing at "
       "anything; answer from the page content or ask."});
  if (kTtcGetPointerUseSelection.Get()) {
    definition.description +=
        " If the user is not pointing but has selected text or an image on "
        "the page, it returns the selection instead, in an image without a "
        "crosshair.";
  }
  // The tool takes no arguments, so its schema is left empty. Like
  // get_screenshot, the model speaks about what the tool returns.
  definition.behavior = ToolDefinition::Behavior::kBlocking;
  definition.verbalization = ToolDefinition::Verbalization::kStandard;
  return definition;
}

void GetPointerTool::Run(ToolResponseCallback callback) {
  callback = base::BindOnce(&GetPointerTool::OnDone, weak_factory_.GetWeakPtr(),
                            base::ElapsedTimer(), std::move(callback));
  content::WebContents* contents =
      session_controller_->GetVoiceFocusedWebContents();
  if (!contents) {
    std::move(callback).Run(ToolResponse::Error(
        actor::mojom::ActionResultCode::kTabWentAway, "No active tab"));
    return;
  }

  // The session has a tracker whenever the tool is offered.
  std::vector<PointerTracker::Location> locations =
      CHECK_DEREF(session_controller_->pointer_tracker())
          .ResolveLocations(session_controller_->user_turn_start(),
                            base::TimeTicks::Now());
  // Whether the user has selected something is only known once the page is
  // captured.
  if (locations.empty() && !kTtcGetPointerUseSelection.Get()) {
    std::move(callback).Run(ToolResponse::Success(
        base::DictValue().Set("status", "no_active_pointer")));
    return;
  }

  content::Page& page = contents->GetPrimaryPage();
  // The session is owned by the TtcKeyedService.
  const TtcKeyedService& ttc_service =
      CHECK_DEREF(TtcKeyedService::Get(session_controller_->GetProfile()));
  IsUrlSupportedForPageContext(
      page.GetMainDocument().GetLastCommittedURL(),
      ttc_service.enterprise_policy_checker(),
      base::BindOnce(&GetPointerTool::OnUrlChecked, weak_factory_.GetWeakPtr(),
                     page.GetWeakPtr(), std::move(locations),
                     std::move(callback)));
}

void GetPointerTool::OnUrlChecked(
    base::WeakPtr<content::Page> page,
    std::vector<PointerTracker::Location> locations,
    ToolResponseCallback callback,
    bool is_url_supported) {
  content::WebContents* web_contents =
      session_controller_->GetVoiceFocusedWebContents();
  if (std::optional<ToolResponse> error =
          CheckPageForCapture(web_contents, page, is_url_supported)) {
    std::move(callback).Run(std::move(*error));
    return;
  }

  capturer_ = std::make_unique<PointerContextCapturer>(
      kTtcGetPointerCaptureTimeout.Get());
  capturer_->Capture(
      web_contents,
      base::BindOnce(&GetPointerTool::OnCaptured, weak_factory_.GetWeakPtr(),
                     std::move(page), std::move(locations),
                     base::ElapsedTimer(), std::move(callback)));
}

void GetPointerTool::OnCaptured(base::WeakPtr<content::Page> page,
                                std::vector<PointerTracker::Location> locations,
                                base::ElapsedTimer capture_timer,
                                ToolResponseCallback callback,
                                PointerContextCapturer::Result context) {
  capturer_.reset();
  if (!context.has_value()) {
    std::move(callback).Run(std::move(context.error()));
    return;
  }
  base::UmaHistogramTimes("Ttc.GetPointer.CaptureLatency",
                          capture_timer.Elapsed());
  // The capturer does not notice the user leaving the page, e.g. for another
  // tab. What was captured is then no longer what they are pointing at, and
  // the page they are on now has not been checked. The URL was found to be
  // supported before the capture.
  if (std::optional<ToolResponse> error = CheckPageForCapture(
          session_controller_->GetVoiceFocusedWebContents(), page,
          /*is_url_supported=*/true)) {
    std::move(callback).Run(std::move(*error));
    return;
  }

  // Making the image is CPU-bound, so not for this thread; the user is waiting
  // on a spoken answer, so not to be put off either.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&BuildGetPointerResponse, std::move(context).value(),
                     std::move(locations),
                     kTtcGetPointerIncludeNodeIdentity.Get()),
      base::BindOnce(&GetPointerTool::OnResponseBuilt,
                     weak_factory_.GetWeakPtr(), std::move(page),
                     std::move(callback)));
}

void GetPointerTool::OnResponseBuilt(base::WeakPtr<content::Page> page,
                                     ToolResponseCallback callback,
                                     ToolResponse response) {
  // Checked once more, since making the image took a while too.
  if (std::optional<ToolResponse> error = CheckPageForCapture(
          session_controller_->GetVoiceFocusedWebContents(), page,
          /*is_url_supported=*/true)) {
    response = std::move(*error);
  } else if (const std::vector<gfx::Point> points =
                 GetPointsInResponse(response);
             !points.empty()) {
    session_controller_->NotifyPointerLocationsReported(points);
  }
  std::move(callback).Run(std::move(response));
}

void GetPointerTool::OnDone(base::ElapsedTimer timer,
                            ToolResponseCallback callback,
                            ToolResponse response) {
  const GetPointerOutcome outcome = ToGetPointerOutcome(response);
  base::UmaHistogramEnumeration("Ttc.GetPointer.Outcome", outcome);

  if (outcome == GetPointerOutcome::kOk) {
    base::UmaHistogramTimes("Ttc.GetPointer.TotalLatency", timer.Elapsed());
    const base::ListValue* points = response.GetResult().FindList("points");
    base::UmaHistogramExactLinear("Ttc.GetPointer.LocationCount",
                                  points ? points->size() : 0, 10);
    const size_t image_bytes = response.visual_content()->image_data.size();
    base::UmaHistogramCounts1M("Ttc.GetPointer.ImageBytes", image_bytes);
    // The journal has every tool call's name, arguments and success already.
    // This adds what the call answered with, less the image.
    session_controller_->GetJournal().Log(
        "TtcGetPointer",
        actor::JournalDetailsBuilder()
            .Add("result",
                 base::WriteJson(response.GetResult()).value_or(std::string()))
            .Add("image_bytes", image_bytes)
            .Build());
  }
  std::move(callback).Run(std::move(response));
}

}  // namespace ttc
