// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/tree_fixing/ax_tree_fixing_services_router.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/accessibility/tree_fixing/internal/ax_tree_fixing_screen_ai_service.h"
#include "chrome/browser/accessibility/tree_fixing/pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_update.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace tree_fixing {

AXTreeFixingServicesRouter::AXTreeFixingWebContentsObserver::
    AXTreeFixingWebContentsObserver(content::WebContents& web_contents,
                                    AXTreeFixingServicesRouter* router)
    : content::WebContentsObserver(&web_contents), router_(router) {}

AXTreeFixingServicesRouter::AXTreeFixingWebContentsObserver::
    ~AXTreeFixingWebContentsObserver() = default;

void AXTreeFixingServicesRouter::AXTreeFixingWebContentsObserver::
    DocumentOnLoadCompletedInPrimaryMainFrame() {
  retry_attempts_ = 0;
  TryIdentifyMainNode();
  router_->IdentifyHeadings(
      web_contents()->RequestAXTreeSnapshotWithinBrowserProcess(),
      web_contents(),
      base::BindOnce(&AXTreeFixingServicesRouter::
                         AXTreeFixingWebContentsObserver::OnHeadingsIdentified,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AXTreeFixingServicesRouter::AXTreeFixingWebContentsObserver::
    TryIdentifyMainNode() {
  bool request_processed = router_->IdentifyMainNode(
      web_contents()->RequestAXTreeSnapshotWithinBrowserProcess(),
      base::BindOnce(&AXTreeFixingServicesRouter::
                         AXTreeFixingWebContentsObserver::OnMainNodeIdentified,
                     weak_ptr_factory_.GetWeakPtr()));

  // If the request was not processed, it likely means that the accessibility
  // engine is still spinning-up (e.g. no root BrowserAccessibilityManager
  // yet). We will retry after a pause, since this is likely a transient
  // state.
  if (!request_processed && retry_attempts_ <= 3) {
    retry_attempts_++;
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AXTreeFixingServicesRouter::
                           AXTreeFixingWebContentsObserver::TryIdentifyMainNode,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Seconds(retry_attempts_));
  }
}

void AXTreeFixingServicesRouter::AXTreeFixingWebContentsObserver::
    OnMainNodeIdentified(const ui::AXTreeID& tree_id, ui::AXNodeID node_id) {
  retry_attempts_ = 0;
  web_contents()->ApplyAXTreeFixingResult(tree_id, node_id,
                                          ax::mojom::Role::kMain);
}

void AXTreeFixingServicesRouter::AXTreeFixingWebContentsObserver::
    OnHeadingsIdentified(const ui::AXTreeID& tree_id,
                         const std::vector<ui::AXNodeID> headings) {
  // TODO: Apply headings to tree.
}

AXTreeFixingServicesRouter::AXTreeFixingServicesRouter(Profile* profile)
    : profile_(profile) {
  CHECK(profile_);
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kAccessibilityAXTreeFixingEnabled,
      base::BindRepeating(&AXTreeFixingServicesRouter::ToggleEnabledState,
                          weak_ptr_factory_.GetWeakPtr()));

  // If the AXTreeFixing feature flag is not enabled, do not initialize.
  if (!features::IsAXTreeFixingEnabled()) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (auto* const accessibility_manager = ash::AccessibilityManager::Get();
      accessibility_manager) {
    accessibility_status_subscription_ =
        accessibility_manager->RegisterCallback(base::BindRepeating(
            &AXTreeFixingServicesRouter::OnAccessibilityStatusEvent,
            weak_ptr_factory_.GetWeakPtr()));
  }
#else
  ax_mode_observation_.Observe(&ui::AXPlatform::GetInstance());
#endif  // BUILDFLAG(IS_CHROMEOS)
  ToggleEnabledState();
}

AXTreeFixingServicesRouter::~AXTreeFixingServicesRouter() = default;

bool AXTreeFixingServicesRouter::IdentifyMainNode(
    const ui::AXTreeUpdate& ax_tree,
    MainNodeIdentificationCallback callback) {
  // This should never be called if the feature is not enabled.
  CHECK(features::IsAXTreeFixingEnabled());

  // If this is the first time any client has requested tree fixing in a form
  // that is handled by the ScreenAI service, then create an instance to connect
  // to the service now.
  if (!screen_ai_service_) {
    screen_ai_service_ =
        std::make_unique<AXTreeFixingScreenAIService>(*this, profile_);
  }

  // If the AXTreeUpdate is empty, do not process the request.
  if (ax_tree.nodes.empty()) {
    return false;
  }

  // We must wait for the ScreenAI service to be ready for requests. We will
  // queue the request for convenience and to keep the services layer obscured
  // from clients.
  if (!can_make_main_node_identification_requests_) {
    // TODO(401308988): Handle queueing requests, or allow observer to have
    // clearer signal of when to make additional requests; pending UX.
    // screen_ai_request_queue_.emplace(ax_tree, std::move(callback));
    return false;
  }

  MakeMainNodeRequestToScreenAI(ax_tree, std::move(callback));
  return true;
}

void AXTreeFixingServicesRouter::MakeMainNodeRequestToScreenAI(
    const ui::AXTreeUpdate& ax_tree,
    MainNodeIdentificationCallback callback) {
  // Store the callback for later use, and make a request to ScreenAI.
  pending_screen_ai_callbacks_.emplace_back(next_screen_ai_request_id_,
                                            std::move(callback));
  screen_ai_service_->IdentifyMainNode(ax_tree, next_screen_ai_request_id_);
  next_screen_ai_request_id_++;
}

void AXTreeFixingServicesRouter::OnMainNodeIdentified(
    const ui::AXTreeID& tree_id,
    ui::AXNodeID node_id,
    int request_id) {
  CHECK(!pending_screen_ai_callbacks_.empty());

  // Find the callback associated with the returned request ID, and call it with
  // the identified tree_id and node_id for the upstream client to use. Remove
  // the pending callback since we have fulfilled the contract.
  for (auto it = pending_screen_ai_callbacks_.begin();
       it != pending_screen_ai_callbacks_.end(); ++it) {
    if (it->first == request_id) {
      MainNodeIdentificationCallback callback = std::move(it->second);
      pending_screen_ai_callbacks_.erase(it);
      std::move(callback).Run(tree_id, node_id);
      return;
    }
  }
  NOTREACHED();
}

void AXTreeFixingServicesRouter::OnServiceStateChanged(bool service_ready) {
  can_make_main_node_identification_requests_ = service_ready;

  // If the service is now ready, process any queued requests.
  if (service_ready) {
    while (!screen_ai_request_queue_.empty()) {
      auto& [ax_tree, callback] = screen_ai_request_queue_.front();
      auto ax_tree_copy = std::move(ax_tree);
      screen_ai_request_queue_.pop();
      MakeMainNodeRequestToScreenAI(ax_tree_copy, std::move(callback));
    }
  }
}

void AXTreeFixingServicesRouter::IdentifyHeadings(
    const ui::AXTreeUpdate& ax_tree_update,
    const raw_ptr<content::WebContents> web_contents,
    HeadingsIdentificationCallback callback) {
  CHECK(features::IsAXTreeFixingEnabled());

  if (!screenshotter_) {
    screenshotter_ = std::make_unique<AXTreeFixingScreenshotter>(*this);
  }

  if (!opt_guide_service_) {
    opt_guide_service_ =
        std::make_unique<AXTreeFixingOptimizationGuideService>(*this, profile_);
  }

  // Store the callback for later use, and make a request to Optimization Guide.
  pending_opt_guide_callbacks_.emplace_back(next_opt_guide_request_id_,
                                            std::move(callback));

  MakeScreenshotRequest(
      web_contents,
      base::BindOnce(
          &AXTreeFixingServicesRouter::OnScreenshotReceivedForHeadings,
          weak_ptr_factory_.GetWeakPtr(), next_opt_guide_request_id_,
          ax_tree_update));

  next_opt_guide_request_id_++;
}

void AXTreeFixingServicesRouter::OnScreenshotReceivedForHeadings(
    int opt_guide_request_id,
    const ui::AXTreeUpdate& ax_tree_update,
    const SkBitmap& bitmap) {
  // Pass the bitmap and the original heading request ID to the service.
  opt_guide_service_->IdentifyHeadings(ax_tree_update, bitmap,
                                       opt_guide_request_id);
}

void AXTreeFixingServicesRouter::MakeScreenshotRequest(
    const raw_ptr<content::WebContents> web_contents,
    ScreenshotCallback callback) {
  // Store the callback for later use, and request a screenshot.
  pending_screenshot_callbacks_.emplace_back(next_screenshot_request_id_,
                                             std::move(callback));
  screenshotter_->RequestScreenshot(web_contents, next_screenshot_request_id_);
  next_screenshot_request_id_++;
}

void AXTreeFixingServicesRouter::OnScreenshotCaptured(const SkBitmap& bitmap,
                                                      int request_id) {
  CHECK(!pending_screenshot_callbacks_.empty());

  // Find the callback associated with the returned request ID, and call it with
  // the screenshot for the upstream client to use. Remove the pending callback
  // since we have fulfilled the contract.
  for (auto it = pending_screenshot_callbacks_.begin();
       it != pending_screenshot_callbacks_.end(); ++it) {
    if (it->first == request_id) {
      ScreenshotCallback callback = std::move(it->second);
      pending_screenshot_callbacks_.erase(it);
      std::move(callback).Run(bitmap);
      return;
    }
  }
  NOTREACHED();
}

void AXTreeFixingServicesRouter::OnHeadingsIdentified(
    const ui::AXTreeID& tree_id,
    const std::vector<ui::AXNodeID> headings,
    int request_id) {
  CHECK(!pending_opt_guide_callbacks_.empty());

  // Find the callback associated with the returned request ID, and call it with
  // the identified tree_id and headings for the upstream client to use. Remove
  // the pending callback since we have fulfilled the contract.
  for (auto it = pending_opt_guide_callbacks_.begin();
       it != pending_opt_guide_callbacks_.end(); ++it) {
    if (it->first == request_id) {
      HeadingsIdentificationCallback callback = std::move(it->second);
      pending_opt_guide_callbacks_.erase(it);
      std::move(callback).Run(tree_id, headings);
      return;
    }
  }
  NOTREACHED();
}

void AXTreeFixingServicesRouter::ToggleEnabledState() {
  // If the AXTreeFixing feature flag is not enabled, do not create observers.
  if (!features::IsAXTreeFixingEnabled()) {
    return;
  }

  // TODO(crbug.com/401308988): Downstream service instances such as
  // screen_ai_service_ need to be cleared when accessibility (or user pref) is
  // disabled.
  web_contents_observers_.clear();
  if (!profile_->GetPrefs()->GetBoolean(
          prefs::kAccessibilityAXTreeFixingEnabled) ||
      !content::BrowserAccessibilityState::GetInstance()
           ->GetAccessibilityModeForBrowserContext(profile_)
           .has_mode(ui::AXMode::kExtendedProperties)) {
    return;
  }
  std::unique_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* rwh = widgets->GetNextHost()) {
    content::RenderViewHost* rvh = content::RenderViewHost::From(rwh);
    if (!rvh) {
      continue;
    }
    content::WebContents* web_contents =
        content::WebContents::FromRenderViewHost(rvh);
    if (!web_contents || web_contents->IsBeingDestroyed()) {
      continue;
    }
    if (web_contents->GetPrimaryMainFrame()->GetRenderViewHost() != rvh) {
      continue;
    }
    web_contents_observers_.push_back(
        std::make_unique<AXTreeFixingWebContentsObserver>(*web_contents, this));
  }
}

#if BUILDFLAG(IS_CHROMEOS)
void AXTreeFixingServicesRouter::OnAccessibilityStatusEvent(
    const ash::AccessibilityStatusEventDetails& details) {
  // We fix all loaded accessibility trees whenever either ChromeVox or
  // Select-toSpeak are turned on.
  if (details.notification_type ==
          ash::AccessibilityNotificationType::kToggleSpokenFeedback ||
      details.notification_type ==
          ash::AccessibilityNotificationType::kToggleSelectToSpeak) {
    ToggleEnabledState();
  }
}
#else   // !BUILDFLAG(IS_CHROMEOS)
void AXTreeFixingServicesRouter::OnAXModeAdded(ui::AXMode mode) {
  if (current_ax_mode_.has_mode(ui::AXMode::kExtendedProperties) !=
      mode.has_mode(ui::AXMode::kExtendedProperties)) {
    current_ax_mode_ = mode;
    ToggleEnabledState();
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace tree_fixing
