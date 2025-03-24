// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/tree_fixing/ax_tree_fixing_services_router.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/accessibility/tree_fixing/internal/ax_tree_fixing_screen_ai_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/accessibility/ax_updates_and_events.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace tree_fixing {

AXTreeFixingServicesRouter::WebContentsObserver::WebContentsObserver(
    content::WebContents& web_contents)
    : content::WebContentsObserver(&web_contents) {}

AXTreeFixingServicesRouter::WebContentsObserver::~WebContentsObserver() =
    default;

void AXTreeFixingServicesRouter::WebContentsObserver::
    AccessibilityEventReceived(const ui::AXUpdatesAndEvents& details) {
  if (!web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame()) {
    return;
  }
  // TODO(crbug.com/401308988): Run fixes here using details.updates.
}

AXTreeFixingServicesRouter::AXTreeFixingServicesRouter(Profile* profile)
    : profile_(profile) {
  CHECK(profile_);
#if BUILDFLAG(IS_CHROMEOS)
  if (auto* const accessibility_manager = ash::AccessibilityManager::Get();
      accessibility_manager) {
    accessibility_status_subscription_ =
        accessibility_manager->RegisterCallback(base::BindRepeating(
            &AXTreeFixingServicesRouter::OnAccessibilityStatusEvent,
            base::Unretained(this)));
  }
#else
  ax_mode_observation_.Observe(&ui::AXPlatform::GetInstance());
#endif  // BUILDFLAG(IS_CHROMEOS)
  ToggleAccessibilityState();
}

AXTreeFixingServicesRouter::~AXTreeFixingServicesRouter() = default;

void AXTreeFixingServicesRouter::IdentifyMainNode(
    const ui::AXTreeUpdate& ax_tree,
    MainNodeIdentificationCallback callback) {
  // If this is the first time any client has requested tree fixing in a form
  // that is handled by the ScreenAI service, then create an instance to connect
  // to the service now.
  if (!screen_ai_service_) {
    screen_ai_service_ =
        std::make_unique<AXTreeFixingScreenAIService>(*this, profile_);
  }

  // We must wait for the ScreenAI service to be ready for requests.
  if (!can_make_main_node_identification_requests_) {
    // TODO(crbug.com/401308988): Give a signal to requesters that they need to
    // re-request? Or, queue requests to be made once service is ready?
    return;
  }

  // Store the callback for later use, and make a request to ScreenAI.
  pending_callbacks_.emplace_back(next_request_id_, std::move(callback));
  screen_ai_service_->IdentifyMainNode(ax_tree, next_request_id_);
  next_request_id_++;
}

#if !BUILDFLAG(IS_CHROMEOS)
void AXTreeFixingServicesRouter::OnAXModeAdded(ui::AXMode mode) {
  if ((!current_ax_mode_.has_mode(ui::AXMode::kExtendedProperties) &&
       mode.has_mode(ui::AXMode::kExtendedProperties)) ||
      (current_ax_mode_.has_mode(ui::AXMode::kExtendedProperties) &&
       !mode.has_mode(ui::AXMode::kExtendedProperties))) {
    current_ax_mode_ = mode;
    ToggleAccessibilityState();
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

void AXTreeFixingServicesRouter::OnMainNodeIdentified(ui::AXTreeID tree_id,
                                                      ui::AXNodeID node_id,
                                                      int request_id) {
  CHECK(!pending_callbacks_.empty());

  // Find the callback associated with the returned request ID, and call it with
  // the identified tree_id and node_id for the upstream client to use. Remove
  // the pending callback since we have fulfilled the contract.
  for (auto it = pending_callbacks_.begin(); it != pending_callbacks_.end();
       ++it) {
    if (it->first == request_id) {
      MainNodeIdentificationCallback callback = std::move(it->second);
      pending_callbacks_.erase(it);
      std::move(callback).Run(std::make_pair(tree_id, node_id));
      return;
    }
  }
  NOTREACHED();
}

void AXTreeFixingServicesRouter::OnServiceStateChanged(bool service_ready) {
  can_make_main_node_identification_requests_ = service_ready;
  // TODO(crbug.com/401308988): Follow-up state change by sending or clearing
  // queued requests (if a queue exists)? Should a signal be sent to clients?
}

void AXTreeFixingServicesRouter::ToggleAccessibilityState() {
  // TODO(crbug.com/401308988): Downstream service instances such as
  // screen_ai_service_ need to be cleared when accessibility (or user pref) is
  // disabled.
  std::unique_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* rwh = widgets->GetNextHost()) {
    content::RenderViewHost* rvh = content::RenderViewHost::From(rwh);
    if (!rvh) {
      continue;
    }
    content::WebContents* web_contents =
        content::WebContents::FromRenderViewHost(rvh);
    if (!web_contents) {
      continue;
    }
    if (web_contents->GetPrimaryMainFrame()->GetRenderViewHost() != rvh) {
      continue;
    }
    if (web_contents->GetAccessibilityMode().has_mode(
            ui::AXMode::kExtendedProperties)) {
      web_contents_observers_.AddObserver(
          new WebContentsObserver(*web_contents));
    } else {
      for (const WebContentsObserver& observer : web_contents_observers_) {
        if (observer.web_contents() == web_contents) {
          web_contents_observers_.RemoveObserver(&observer);
          break;
        }
      }
    }
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
    ToggleAccessibilityState();
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace tree_fixing
