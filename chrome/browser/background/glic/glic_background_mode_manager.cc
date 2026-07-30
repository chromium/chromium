// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/glic/glic_background_mode_manager.h"

#include <memory>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/background/glic/glic_status_icon.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/common/local_hotkey_manager.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/nuke_profile_directory_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/startup/startup_launch_manager.h"
#endif

namespace glic {

class GlicBackgroundModeManager::AcceleratorRegistrar
    : public ui::GlobalAcceleratorListener::Observer {
 public:
  AcceleratorRegistrar(GlicBackgroundModeManager* manager,
                       const std::vector<ui::Accelerator>& accelerators)
      : manager_(CHECK_DEREF(manager)) {
    auto* const global_accelerator_listener =
        ui::GlobalAcceleratorListener::GetInstance();
    manager->actual_registered_hotkeys_.clear();
    if (global_accelerator_listener) {
      const bool shortcut_handling_suspended =
          global_accelerator_listener->IsShortcutHandlingSuspended();
      // Re-enable shortcut handling to allow the global accelerator listener to
      // register the hotkey.
      global_accelerator_listener->SetShortcutHandlingSuspended(false);
      for (auto accelerator : accelerators) {
        if (!accelerator.IsEmpty() &&
            !global_accelerator_listener->RegisterAccelerator(accelerator,
                                                              this)) {
          accelerator = ui::Accelerator();
        }
        manager->actual_registered_hotkeys_.push_back(accelerator);
      }
      global_accelerator_listener->SetShortcutHandlingSuspended(
          shortcut_handling_suspended);
    }
  }

  ~AcceleratorRegistrar() override {
    auto* const global_accelerator_listener =
        ui::GlobalAcceleratorListener::GetInstance();
    if (global_accelerator_listener) {
      global_accelerator_listener->UnregisterAccelerators(this);
    }
    manager_->actual_registered_hotkeys_.clear();
  }

  // ui::GlobalAcceleratorListener::Observer
  void OnKeyPressed(const ui::Accelerator& accelerator) override {
    manager_->HandleHotkey(accelerator);
  }

  // ui::GlobalAcceleratorListener::Observer
  void ExecuteCommand(const std::string& accelerator_group_id,
                      const std::string& command_id) override {
    // TODO(crbug.com/385194502): Handle Linux.
  }

 private:
  const raw_ref<GlicBackgroundModeManager> manager_;
};

GlicBackgroundModeManager::GlicBackgroundModeManager(StatusTray* status_tray)
    : configuration_(std::make_unique<GlicLauncherConfiguration>(this)),
      status_tray_(status_tray),
      enabled_pref_(GlicLauncherConfiguration::IsEnabled()),
      expected_registered_hotkeys_(
          ShouldRegisterGlobalHotkey()
              ? std::vector<ui::Accelerator>{GlicLauncherConfiguration::
                                                 GetToggleHotkey()}
              : std::vector<ui::Accelerator>{}) {
  g_browser_process->profile_manager()->AddObserver(this);
  // Start tracking any profiles that already exist.
  for (auto* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    OnProfileAdded(profile);
  }
  UpdateState();
}

GlicBackgroundModeManager::~GlicBackgroundModeManager() {
  g_browser_process->profile_manager()->RemoveObserver(this);
}

GlicBackgroundModeManager* GlicBackgroundModeManager::GetInstance() {
  return g_browser_process->GetFeatures()->glic_background_mode_manager();
}

void GlicBackgroundModeManager::OnEnabledChanged(bool enabled) {
  if (enabled_pref_ == enabled) {
    return;
  }

  enabled_pref_ = enabled;
  UpdateExpectedHotkeys();
  UpdateState();
}

void GlicBackgroundModeManager::OnGlobalHotkeyChanged() {
  if (UpdateExpectedHotkeys()) {
    UpdateState();
  } else if (status_icon_) {
    status_icon_->UpdateHotkey(GetHotkeyToShow());
  }
}

bool GlicBackgroundModeManager::UpdateExpectedHotkeys() {
  std::vector<ui::Accelerator> new_hotkeys;
  if (ShouldRegisterGlobalHotkey()) {
    new_hotkeys.push_back(GlicLauncherConfiguration::GetToggleHotkey());
  }
  if (expected_registered_hotkeys_ == new_hotkeys) {
    return false;
  }
  expected_registered_hotkeys_ = std::move(new_hotkeys);
  return true;
}

ui::Accelerator GlicBackgroundModeManager::GetHotkeyToShow() const {
  if (ShouldRegisterGlobalHotkey()) {
    return actual_registered_hotkeys_.empty()
               ? ui::Accelerator()
               : actual_registered_hotkeys_.at(
                     static_cast<size_t>(HotkeyIndex::kPanelKey));
  }
  return GlicLauncherConfiguration::GetToggleHotkey();
}

void GlicBackgroundModeManager::ToggleUI(bool prevent_close,
                                         mojom::InvocationSource source) {
  Profile* profile = GlicProfileManager::GetInstance()->GetProfileForLaunch();
  if (!profile) {
    return;
  }

  GlicKeyedService* glic_keyed_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  glic_keyed_service->ToggleUI(nullptr, prevent_close, source);
}

void GlicBackgroundModeManager::HandleHotkey(
    const ui::Accelerator& accelerator) {
  auto it = std::find(actual_registered_hotkeys_.begin(),
                      actual_registered_hotkeys_.end(), accelerator);
  CHECK(it != actual_registered_hotkeys_.end());

  switch (static_cast<HotkeyIndex>(
      std::distance(actual_registered_hotkeys_.begin(), it))) {
    case HotkeyIndex::kPanelKey: {
      ToggleUI(/*prevent_close=*/false, mojom::InvocationSource::kOsHotkey);
      // Record hotkey usage.
      const ui::Accelerator default_hotkey =
          LocalHotkeyManager::GetDefaultAccelerator(
              LocalHotkeyManager::Command::kPanelToggle);
      base::UmaHistogramEnumeration("Glic.Usage.Hotkey",
                                    accelerator == default_hotkey
                                        ? glic::HotkeyUsage::kDefault
                                        : glic::HotkeyUsage::kCustom);
      break;
    }
  }
}

void GlicBackgroundModeManager::OnProfileAdded(Profile* profile) {
  auto* service = GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  if (!service) {
    return;
  }
  // Recompute whether the background launcher should change state based on the
  // updated policy and FRE completion state.
  GlicEnabling& enabling = service->enabling();
  profile_enabled_subscriptions_.emplace(
      profile, enabling.RegisterAllowedChanged(
                   base::BindRepeating(&GlicBackgroundModeManager::UpdateState,
                                       weak_ptr_factory_.GetWeakPtr())));
  profile_consent_subscriptions_.emplace(
      profile, enabling.RegisterOnConsentChanged(
                   base::BindRepeating(&GlicBackgroundModeManager::UpdateState,
                                       weak_ptr_factory_.GetWeakPtr())));
  auto [it, inserted] = profile_observers_.emplace(profile, this);
  it->second.Observe(profile);

  // If a profile is added when not in background mode, check if it can now be
  // entered.
  if (!keep_alive_) {
    UpdateState();
  }
}

void GlicBackgroundModeManager::OnProfileWillBeDestroyed(Profile* profile) {
  profile_observers_.erase(profile);
  profile_enabled_subscriptions_.erase(profile);
  profile_consent_subscriptions_.erase(profile);

  // If a profile is removed while in background mode, check if it must now be
  // exited.
  if (keep_alive_) {
    UpdateState();
  }
}

void GlicBackgroundModeManager::Shutdown() {
  CHECK(g_browser_process->profile_manager());
  g_browser_process->profile_manager()->RemoveObserver(this);
}

void GlicBackgroundModeManager::EnterBackgroundMode(bool show_status_icon) {
  KeepAliveRegistry* const keep_alive_registry =
      KeepAliveRegistry::GetInstance();
  if (!keep_alive_ && keep_alive_registry &&
      !keep_alive_registry->IsShuttingDown()) {
    keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::GLIC_LAUNCHER, KeepAliveRestartOption::ENABLED);
  }

  if (show_status_icon) {
    if (!status_icon_) {
      status_icon_ = GlicStatusIcon::Create(this, status_tray_);
      status_icon_->Init();
    }
  } else {
    status_icon_.reset();
  }
}

void GlicBackgroundModeManager::ExitBackgroundMode() {
  status_icon_.reset();
  keep_alive_.reset();
}

void GlicBackgroundModeManager::RegisterHotkeys(
    const std::vector<ui::Accelerator>& updated_hotkeys) {
  CHECK(!accelerator_registrar_);
  accelerator_registrar_ =
      std::make_unique<AcceleratorRegistrar>(this, updated_hotkeys);
}

void GlicBackgroundModeManager::UnregisterHotkey() {
  accelerator_registrar_.reset();
}

void GlicBackgroundModeManager::UpdateState() {
  UnregisterHotkey();

  bool any_profile_enabled = IsEnabledInAnyLoadedProfile();
  bool register_global_hotkeys =
      any_profile_enabled && ShouldRegisterGlobalHotkey();

  // We need to keep Chrome alive if launcher is enabled OR global hotkeys need
  // to be registered.
  bool should_be_active =
      any_profile_enabled && (enabled_pref_ || ShouldRegisterGlobalHotkey());

  if (should_be_active) {
    EnterBackgroundMode(
        /*show_status_icon=*/enabled_pref_ && any_profile_enabled);
    if (register_global_hotkeys) {
      RegisterHotkeys(expected_registered_hotkeys_);
    }
    if (status_icon_) {
      status_icon_->UpdateHotkey(GetHotkeyToShow());
    }
  } else {
    ExitBackgroundMode();
  }

#if BUILDFLAG(IS_WIN)
  startup_launch_client_.SetLaunchOnStartup(should_be_active);
#endif
}

bool GlicBackgroundModeManager::IsEnabledInAnyLoadedProfile() {
  for (const auto& pair : profile_observers_) {
    Profile* profile = pair.first;
    if (!IsProfileDirectoryMarkedForDeletion(profile->GetPath()) &&
        glic::GlicEnabling::IsEnabledAndConsentForProfile(profile)) {
      return true;
    }
  }
  return false;
}

bool GlicBackgroundModeManager::ShouldRegisterGlobalHotkey() const {
  if (GlicLauncherConfiguration::GetToggleHotkey().IsEmpty()) {
    return false;
  }
  if (!base::FeatureList::IsEnabled(features::kGlicHotkeyLocalScope)) {
    return enabled_pref_;
  }
  return g_browser_process->local_state() &&
         g_browser_process->local_state()->GetBoolean(
             prefs::kGlicHotkeyGlobalScopeEnabled);
}

}  // namespace glic
