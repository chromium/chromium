// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/ax_screen_ai_annotator.h"

#include <utility>

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"

#if defined(USE_AURA)
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event.h"
#include "ui/aura/env.h"
#include "ui/gfx/geometry/point.h"
#endif  // defined(USE_AURA)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
namespace {

gfx::Image GrabViewSnapshot(
    base::WeakPtr<content::WebContents> web_contents_ptr) {
  gfx::Image snapshot;
  // Need to return an empty image if the WebContents got destroyed before
  // taking a screenshot or it failed to take a screenshot.
  if (!web_contents_ptr ||
      !ui::GrabViewSnapshot(web_contents_ptr->GetContentNativeView(),
                            gfx::Rect(web_contents_ptr->GetSize()), &snapshot))
    snapshot = gfx::Image();

  return snapshot;
}

}  // namespace
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

namespace screen_ai {

AXScreenAIAnnotator::AXScreenAIAnnotator(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context), screen_ai_service_client_(this) {
  component_ready_observer_.Observe(ScreenAIInstallState::GetInstance());
}

AXScreenAIAnnotator::~AXScreenAIAnnotator() = default;

void AXScreenAIAnnotator::StateChanged(ScreenAIInstallState::State state) {
  if (state != ScreenAIInstallState::State::kReady &&
      state != ScreenAIInstallState::State::kDownloaded) {
    return;
  }

  if (!screen_ai_service_client_.is_bound()) {
    BindToScreenAIService(browser_context_);
  }
}

void AXScreenAIAnnotator::BindToScreenAIService(
    content::BrowserContext* browser_context) {
  mojo::PendingReceiver<mojom::ScreenAIAnnotator> screen_ai_receiver =
      screen_ai_annotator_.BindNewPipeAndPassReceiver();

  ScreenAIServiceRouter* service_router =
      ScreenAIServiceRouterFactory::GetForBrowserContext(browser_context);

  // Client interface should be ready to receive annotation results before a
  // request is sent to the service, therefore it should be created first.
  service_router->BindScreenAIAnnotatorClient(
      screen_ai_service_client_.BindNewPipeAndPassRemote());
  service_router->BindScreenAIAnnotator(std::move(screen_ai_receiver));
}

void AXScreenAIAnnotator::AnnotateScreenshot(
    content::WebContents* web_contents) {
  // Request screenshot from content area of the main frame.
  if (!web_contents)
    return;
  gfx::NativeView native_view = web_contents->GetContentNativeView();
  if (!native_view)
    return;
  if (!web_contents->GetPrimaryMainFrame())
    return;

  base::TimeTicks start_time = base::TimeTicks::Now();
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // TODO(https://crbug.com/1443349): Need to run GrabViewSnapshot() in a
  // thread that is not the main UI thread.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&GrabViewSnapshot, web_contents->GetWeakPtr()),
      base::BindOnce(&AXScreenAIAnnotator::OnScreenshotReceived,
                     weak_ptr_factory_.GetWeakPtr(),
                     web_contents->GetPrimaryMainFrame()->GetAXTreeID(),
                     start_time));
#else
  ui::GrabViewSnapshotAsync(
      native_view, gfx::Rect(web_contents->GetSize()),
      base::BindOnce(&AXScreenAIAnnotator::OnScreenshotReceived,
                     weak_ptr_factory_.GetWeakPtr(),
                     web_contents->GetPrimaryMainFrame()->GetAXTreeID(),
                     start_time));
#endif
}

void AXScreenAIAnnotator::OnScreenshotReceived(
    const ui::AXTreeID& ax_tree_id,
    const base::TimeTicks& start_time,
    gfx::Image snapshot) {
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  if (snapshot.IsEmpty()) {
    VLOG(1) << "AxScreenAIAnnotator could not grab snapshot.";
    base::UmaHistogramTimes(
        "Accessibility.ScreenAI.AnnotateScreenshotTime.Failure", elapsed_time);
    return;
  }

  base::UmaHistogramTimes(
      "Accessibility.ScreenAI.AnnotateScreenshotTime.Success", elapsed_time);

  // If screenshot is ready before service is initialized, the service call is
  // delayed for 3 seconds.
  // TODO(crbug.com/1443349): This solution is only for prototyping and should
  // be updated so that the requests are queued before initialization
  // completes.
  if (!screen_ai_annotator_.is_bound() ||
      !screen_ai_service_client_.is_bound()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AXScreenAIAnnotator::ExtractSemanticLayout,
                       weak_ptr_factory_.GetWeakPtr(), ax_tree_id,
                       snapshot.AsBitmap()),
        base::Seconds(3));
  } else {
    ExtractSemanticLayout(ax_tree_id, snapshot.AsBitmap());
  }
}

void AXScreenAIAnnotator::ExtractSemanticLayout(const ui::AXTreeID& ax_tree_id,
                                                const SkBitmap bitmap) {
  if (!screen_ai_annotator_.is_bound() ||
      !screen_ai_service_client_.is_bound()) {
    VLOG(0) << "Service is not ready yet.";
    return;
  }

  screen_ai_annotator_->ExtractSemanticLayout(
      bitmap, ax_tree_id,
      base::BindOnce(&AXScreenAIAnnotator::OnSemanticLayoutExtractionPerformed,
                     weak_ptr_factory_.GetWeakPtr(), ax_tree_id));
}

void AXScreenAIAnnotator::OnSemanticLayoutExtractionPerformed(
    const ui::AXTreeID& parent_tree_id,
    const ui::AXTreeID& screen_ai_tree_id) {
  VLOG(2) << base::StrCat({"AXScreenAIAnnotator received tree ids: parent: ",
                           parent_tree_id.ToString().c_str(), ", ScreenAI: ",
                           screen_ai_tree_id.ToString().c_str()});
  // TODO(https://crbug.com/1443349): Use!
  NOTIMPLEMENTED();
}

void AXScreenAIAnnotator::HandleAXTreeUpdate(const ui::AXTreeUpdate& update) {
  VLOG(2) << "HandleAXTreeUpdate:\n" << update.ToString();
  DCHECK(update.has_tree_data);
  DCHECK_NE(update.tree_data.tree_id, ui::AXTreeIDUnknown());
  tree_ids_.push_back(update.tree_data.tree_id);
  DCHECK_NE(ui::kInvalidAXNodeID, update.root_id);

#if defined(USE_AURA)
  auto* event_router = extensions::AutomationEventRouter::GetInstance();
  DCHECK(event_router);
  const gfx::Point& mouse_location =
      aura::Env::GetInstance()->last_mouse_location();
  event_router->DispatchAccessibilityEvents(
      update.tree_data.tree_id, {update}, mouse_location,
      {ui::AXEvent(update.root_id, ax::mojom::Event::kLayoutComplete,
                   ax::mojom::EventFrom::kNone)});
#endif  // defined(USE_AURA)
}

}  // namespace screen_ai
