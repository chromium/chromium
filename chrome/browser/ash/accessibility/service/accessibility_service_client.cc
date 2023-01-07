// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/service/accessibility_service_client.h"
#include <memory>
#include "base/callback_helpers.h"
#include "chrome/browser/accessibility/service/accessibility_service_router.h"
#include "chrome/browser/accessibility/service/accessibility_service_router_factory.h"
#include "chrome/browser/ash/accessibility/service/automation_client_impl.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom-shared.h"

namespace ash {

AccessibilityServiceClient::AccessibilityServiceClient() = default;

AccessibilityServiceClient::~AccessibilityServiceClient() {
  Reset();
}

void AccessibilityServiceClient::SetProfile(content::BrowserContext* profile) {
  // If the profile has changed we will need to disconnect from the previous
  // service, get the service keyed to this profile, and if any features were
  // enabled, re-establish the service connection with those features. Note that
  // this matches behavior in AccessibilityExtensionLoader::SetProfile, which
  // does the parallel logic with the extension system.
  if (profile_ == profile)
    return;

  Reset();
  profile_ = profile;
  if (profile_ && enabled_features_.size())
    LaunchAccessibilityServiceAndBind();
}

void AccessibilityServiceClient::SetChromeVoxEnabled(bool enabled) {
  EnableAssistiveTechnology(ax::mojom::AssistiveTechnologyType::kChromeVox,
                            enabled);
}

void AccessibilityServiceClient::SetSelectToSpeakEnabled(bool enabled) {
  EnableAssistiveTechnology(ax::mojom::AssistiveTechnologyType::kSelectToSpeak,
                            enabled);
}

void AccessibilityServiceClient::SetSwitchAccessEnabled(bool enabled) {
  EnableAssistiveTechnology(ax::mojom::AssistiveTechnologyType::kSwitchAccess,
                            enabled);
}

void AccessibilityServiceClient::SetAutoclickEnabled(bool enabled) {
  EnableAssistiveTechnology(ax::mojom::AssistiveTechnologyType::kAutoClick,
                            enabled);
}

void AccessibilityServiceClient::SetMagnifierEnabled(bool enabled) {
  EnableAssistiveTechnology(ax::mojom::AssistiveTechnologyType::kMagnifier,
                            enabled);
}

void AccessibilityServiceClient::SetDictationEnabled(bool enabled) {
  EnableAssistiveTechnology(ax::mojom::AssistiveTechnologyType::kDictation,
                            enabled);
}

void AccessibilityServiceClient::Reset() {
  at_controller_.reset();
  automation_client_.reset();
}

void AccessibilityServiceClient::EnableAssistiveTechnology(
    ax::mojom::AssistiveTechnologyType type,
    bool enabled) {
  // Update the list of enabled features.
  auto iter =
      std::find(enabled_features_.begin(), enabled_features_.end(), type);
  if (enabled && iter == enabled_features_.end()) {
    enabled_features_.push_back(type);
  } else if (!enabled && iter != enabled_features_.end()) {
    enabled_features_.erase(iter);
  }

  if (!enabled && !at_controller_.is_bound()) {
    // No need to launch the service, nothing is enabled.
    return;
  }

  if (at_controller_.is_bound()) {
    // TODO(crbug.com/1355633): Enable assistive technology with mojom.
    // at_controller_->EnableAssistiveTechnology(type, enabled);
    return;
  }

  // A new feature is enabled but the service isn't running yet.
  LaunchAccessibilityServiceAndBind();
}

void AccessibilityServiceClient::LaunchAccessibilityServiceAndBind() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!profile_)
    return;

  ax::AccessibilityServiceRouter* router =
      ax::AccessibilityServiceRouterFactory::GetForBrowserContext(
          static_cast<content::BrowserContext*>(profile_));
  automation_client_ = std::make_unique<AutomationClientImpl>();
  automation_client_->Bind(router);
  router->BindAssistiveTechnologyController(
      at_controller_.BindNewPipeAndPassReceiver(), enabled_features_);
}

}  // namespace ash
