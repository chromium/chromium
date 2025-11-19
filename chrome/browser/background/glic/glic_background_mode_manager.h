// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_GLIC_GLIC_BACKGROUND_MODE_MANAGER_H_
#define CHROME_BROWSER_BACKGROUND_GLIC_GLIC_BACKGROUND_MODE_MANAGER_H_

#include <map>
#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"

class ScopedKeepAlive;
class StatusTray;

namespace ui {
class Accelerator;
}

namespace glic {

class GlicController;
class GlicStatusIcon;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(HotkeyUsage)
enum class HotkeyUsage {
  kDefault = 0,
  kCustom = 1,
  kMaxValue = kCustom,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicHotkeyUsage)

// This is a global feature in the browser process that manages the
// enabling/disabling of glic background mode. When background mode is enabled,
// chrome is set to keep alive the browser process, so that this class can
// listen to a global hotkey, and provide a status icon for triggering the UI.
class GlicBackgroundModeManager : public GlicLauncherConfiguration::Observer,
                                  public ProfileManagerObserver,
                                  public ProfileObserver {
 public:
  explicit GlicBackgroundModeManager(StatusTray* status_tray);
  ~GlicBackgroundModeManager() override;

  static GlicBackgroundModeManager* GetInstance();

  // GlicConfiguration::Observer
  void OnEnabledChanged(bool enabled) override;
  void OnGlobalHotkeyChanged(ui::Accelerator hotkey) override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  void HandleHotkey(const ui::Accelerator& accelerator);

  void Shutdown();

  ui::Accelerator RegisteredHotkeyForTesting() {
    return actual_registered_hotkey_;
  }

  bool IsInBackgroundModeForTesting() {
    CHECK_EQ(static_cast<bool>(keep_alive_), static_cast<bool>(status_icon_));
    return keep_alive_ != nullptr;
  }

  GlicStatusIcon* GetStatusIconForTesting() { return status_icon_.get(); }

  void EnterBackgroundMode();
  void ExitBackgroundMode();

 private:
  class AcceleratorRegistrar;

  void EnableLaunchOnStartup(bool should_launch);
  void RegisterHotkey(ui::Accelerator updated_hotkey);
  void UnregisterHotkey();
  void UpdateState();

  bool IsEnabledInAnyLoadedProfile();

  // A helper class for observing pref changes.
  std::unique_ptr<GlicLauncherConfiguration> configuration_;

  // An abstraction used to show/hide the UI.
  std::unique_ptr<GlicController> controller_;

  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  // TODO(https://crbug.com/378139555): Figure out how to not dangle this
  // pointer (and other instances of StatusTray).
  raw_ptr<StatusTray, DanglingUntriaged> status_tray_;
  // Class that represents the glic status icon. Only exists when the background
  // mode is enabled.
  std::unique_ptr<GlicStatusIcon> status_icon_;

  // The current state of the launcher_enabled pref. Note that the pref is a
  // local state and is thus per-installation. Each profile also has a
  // "settings_policy" pref which can be used to disable the feature for a
  // profile. Background mode is entered only if `enabled_pref` is true AND at
  // least one loaded profile is enabled by policy.
  bool enabled_pref_ = false;

  // The actual registered hotkey may be different from the expected hotkey
  // because the Glic launcher may be disabled or registration fails which
  // results in no hotkey being registered and is represented with an empty
  // accelerator.
  ui::Accelerator expected_registered_hotkey_;
  ui::Accelerator actual_registered_hotkey_;

  // Accelerator subclass to control accelerator registration between different
  // platform.
  std::unique_ptr<AcceleratorRegistrar> accelerator_registrar_;

  // Listens to changes to IsEnabled() for profiles.
  std::map<Profile*, base::CallbackListSubscription>
      profile_enabled_subscriptions_;
  std::map<Profile*, base::CallbackListSubscription>
      profile_consent_subscriptions_;
  using ScopedProfileObserver =
      base::ScopedObservation<Profile, ProfileObserver>;
  std::map<Profile*, ScopedProfileObserver> profile_observers_;
  base::WeakPtrFactory<GlicBackgroundModeManager> weak_ptr_factory_{this};
};
}  // namespace glic

#endif  // CHROME_BROWSER_BACKGROUND_GLIC_GLIC_BACKGROUND_MODE_MANAGER_H_
