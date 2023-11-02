// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DISPLAY_DISPLAY_RESOLUTION_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_DISPLAY_DISPLAY_RESOLUTION_HANDLER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "chrome/browser/ash/policy/display/display_settings_handler.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom-forward.h"

namespace policy {

// Implements DeviceDisplayResolution device policy.
//
// Whenever there is a change in the display configration, any new display will
// be resized according to the policy (only if the policy is enabled and the
// display supports specified resolution and scale factor).
//
// Whenever there is a change in |kDeviceDisplayResolution| setting from
// CrosSettings, the new policy is reapplied to all displays.
//
// If the specified resolution or scale factor is not supported by some display,
// the resolution won't change.
//
// Once resolution or scale factor for some display was set by this policy it
// won't be reapplied until next reboot or policy change (i.e. user can manually
// override the settings for that display via settings page).
class DisplayResolutionHandler : public DisplaySettingsPolicyHandler {
 public:
  DisplayResolutionHandler();

  DisplayResolutionHandler(const DisplayResolutionHandler&) = delete;
  DisplayResolutionHandler& operator=(const DisplayResolutionHandler&) = delete;

  ~DisplayResolutionHandler() override;

  // DisplaySettingsPolicyHandler
  const char* SettingName() override;
  void OnSettingUpdate() override;
  void ApplyChanges(
      crosapi::mojom::CrosDisplayConfigController* cros_display_config,
      const std::vector<crosapi::mojom::DisplayUnitInfoPtr>& info_list)
      override;

 private:
  struct InternalDisplaySettings;
  struct ExternalDisplaySettings;

  bool policy_enabled_ = false;
  bool recommended_ = false;
  std::unique_ptr<ExternalDisplaySettings> external_display_settings_;
  std::unique_ptr<InternalDisplaySettings> internal_display_settings_;
  std::set<std::string> resized_display_ids_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DISPLAY_DISPLAY_RESOLUTION_HANDLER_H_
