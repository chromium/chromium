// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DISPLAY_DISPLAY_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_DISPLAY_DISPLAY_SETTINGS_HANDLER_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace policy {

// Provides an interface for display configuration policies. Each policy should
// provide a handler that implements this interface and register it in
// the |DisplaySettingsHandler| instance.
class DisplaySettingsPolicyHandler {
 public:
  virtual ~DisplaySettingsPolicyHandler() = default;

  // Returns a name of setting stored in CrosSettings that should be used to
  // subscribe to setting changes for this policy.
  virtual const char* SettingName() = 0;

  // Is called on each setting update before |ApplyChanges| to provide
  // updated settings for the handler.
  virtual void OnSettingUpdate() = 0;

  // Applies settings enforced by the policy to each display from |info_list|.
  // Is called on each configuration change or settings update.
  virtual void ApplyChanges(
      crosapi::mojom::CrosDisplayConfigController* cros_display_config,
      const std::vector<crosapi::mojom::DisplayUnitInfoPtr>& info_list) = 0;
};

// Enforces the settings controlled by device policies related to display
// configuration (i.e. DisplayRotationDefault, DeviceDisplayResolution)
// On construction this class registers itself with
// crosapi::mojom::CrosDisplayConfigObserver for display changes and with
// CrosSettings for settings changes. Every display configuration policy
// provides a handler class inherited from |DisplaySettingsPolicyHandler|
// and is registered in |DisplaySettingsHandler| instance.
// see |DisplayResolutionHandler| and |DisplayRotationDefaultHandler|
class DisplaySettingsHandler
    : public crosapi::mojom::CrosDisplayConfigObserver {
 public:
  // This class must be constructed after CrosSettings is initialized.
  DisplaySettingsHandler();

  DisplaySettingsHandler(const DisplaySettingsHandler&) = delete;
  DisplaySettingsHandler& operator=(const DisplaySettingsHandler&) = delete;

  ~DisplaySettingsHandler() override;

  // crosapi::mojom::CrosDisplayConfigObserver
  void OnDisplayConfigChanged() override;

  // Registers handler for some policy-controlled setting. All handlers must be
  // registered before calling |Start|.
  void RegisterHandler(std::unique_ptr<DisplaySettingsPolicyHandler> handler);

  // Subscribes to all needed events and initiates initial display configuration
  // fetching to apply the settings controlled by registered handlers.
  void Start();

 private:
  // Receives the initial display info list and initializes the class.
  void OnGetInitialDisplayInfo(
      std::vector<crosapi::mojom::DisplayUnitInfoPtr> info_list);

  // Requests the list of displays and applies each setting.
  void RequestDisplaysAndApplyChanges();

  // Apply all default settings defined by policies to all connected displays.
  void ApplyChanges(std::vector<crosapi::mojom::DisplayUnitInfoPtr> info_list);

  // Called on each update of the setting provided by |handler|. Requests the
  // list of displays and applies |handler| to each display.
  void OnSettingUpdate(DisplaySettingsPolicyHandler* handler);

  // Applies |handler| to each display from |info_list|.
  void UpdateSettingAndApplyChanges(
      DisplaySettingsPolicyHandler* handler,
      const std::vector<crosapi::mojom::DisplayUnitInfoPtr>& info_list);

  // Called on display configuration changes for each handler.
  void OnConfigurationChangeForHandler(
      DisplaySettingsPolicyHandler* handler,
      std::vector<crosapi::mojom::DisplayUnitInfoPtr> info_list);

  // Provides access to the current display configurations, both for reading and
  // updating.
  mojo::Remote<crosapi::mojom::CrosDisplayConfigController>
      cros_display_config_;
  std::vector<std::unique_ptr<DisplaySettingsPolicyHandler>> handlers_;
  mojo::AssociatedReceiver<crosapi::mojom::CrosDisplayConfigObserver>
      cros_display_config_observer_receiver_{this};
  std::vector<base::CallbackListSubscription> settings_subscriptions_;
  bool started_ = false;

  // Must be the last member.
  base::WeakPtrFactory<DisplaySettingsHandler> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DISPLAY_DISPLAY_SETTINGS_HANDLER_H_
