// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ECHE_APP_ECHE_APP_ACCESSIBILITY_PROVIDER_PROXY_H_
#define CHROME_BROWSER_ASH_ECHE_APP_ECHE_APP_ACCESSIBILITY_PROVIDER_PROXY_H_

#include "ash/webui/eche_app_ui/accessibility_provider.h"

#include "chrome/browser/ash/accessibility/accessibility_manager.h"

namespace ash::eche_app {

class EcheAppAccessibilityProviderProxy : public AccessibilityProviderProxy {
 public:
  EcheAppAccessibilityProviderProxy();
  ~EcheAppAccessibilityProviderProxy() override;

  // Accessibility Interactions
  void OnAccessibilityStatusChanged(
      const ash::AccessibilityStatusEventDetails& event_details);

  // Proxy Overrides
  bool UseFullFocusMode() override;
  bool IsAccessibilityEnabled() override;
  ax::android::mojom::AccessibilityFilterType GetFilterType() override;
  void OnViewTracked() override;
  void SetAccessibilityEnabledStateChangedCallback(
      base::RepeatingCallback<void(bool)> callback) override;
  void SetExploreByTouchEnabledStateChangedCallback(
      base::RepeatingCallback<void(bool)> callback) override;

 private:
  void UpdateEnabledFeature();
  base::CallbackListSubscription accessibility_status_subscription_;
  bool use_full_focus_mode_ = false;
  bool was_accessibility_enabled_;

  std::optional<base::RepeatingCallback<void(bool)>>
      accessibility_state_changed_callback_;
  std::optional<base::RepeatingCallback<void(bool)>>
      explore_by_touch_state_changed_callback_;
  base::WeakPtrFactory<EcheAppAccessibilityProviderProxy> weak_ptr_factory_{
      this};
};
}  // namespace ash::eche_app
#endif  // CHROME_BROWSER_ASH_ECHE_APP_ECHE_APP_ACCESSIBILITY_PROVIDER_PROXY_H_
