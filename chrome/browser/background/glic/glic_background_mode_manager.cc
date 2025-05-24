// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/glic/glic_background_mode_manager.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/background/glic/glic_controller.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/background/glic/glic_status_icon.h"
#include "chrome/browser/background/startup_launch_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/nuke_profile_directory_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"

namespace glic {

GlicBackgroundModeManager::GlicBackgroundModeManager(StatusTray* status_tray)
    : configuration_(std::make_unique<GlicLauncherConfiguration>(this)),
      controller_(std::make_unique<GlicController>()),
      status_tray_(status_tray),
      enabled_pref_(GlicLauncherConfiguration::IsEnabled()),
      expected_registered_hotkey_(
          GlicLauncherConfiguration::GetGlobalHotkey()) {
  g_browser_process->profile_manager()->AddObserver(this);
  // Start tracking any profiles that already exist.
  for (auto* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    OnProfileAdded(profile);
  }
  EnableLaunchOnStartup(enabled_pref_);
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
  UpdateState();
  EnableLaunchOnStartup(enabled_pref_);
}

void GlicBackgroundModeManager::OnGlobalHotkeyChanged(ui::Accelerator hotkey) {
  if (expected_registered_hotkey_ == hotkey) {
    return;
  }

  expected_registered_hotkey_ = hotkey;
  UpdateState();
}

void GlicBackgroundModeManager::OnKeyPressed(
    const ui::Accelerator& accelerator) {
  CHECK(accelerator == actual_registered_hotkey_);
  CHECK(actual_registered_hotkey_ == expected_registered_hotkey_);
  controller_->Toggle(mojom::InvocationSource::kOsHotkey);
  // Record hotkey usage.
  const ui::Accelerator default_hotkey =
      GlicLauncherConfiguration::GetDefaultHotkey();
  base::UmaHistogramEnumeration("Glic.Usage.Hotkey",
                                accelerator == default_hotkey
                                    ? glic::HotkeyUsage::kDefault
                                    : glic::HotkeyUsage::kCustom);
}

void GlicBackgroundModeManager::ExecuteCommand(
    const std::string& accelerator_group_id,
    const std::string& command_id) {
  // TODO(crbug.com/385194502): Handle Linux.
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
  if (!status_icon_) {
    CHECK(!keep_alive_);
    UpdateState();
  }
}

void GlicBackgroundModeManager::OnProfileWillBeDestroyed(Profile* profile) {
  profile_observers_.erase(profile);
  profile_enabled_subscriptions_.erase(profile);
  profile_consent_subscriptions_.erase(profile);

  // If a profile is removed while in background mode, check if it must now be
  // exited.
  if (status_icon_) {
    UpdateState();
  }
}

void GlicBackgroundModeManager::Shutdown() {
  CHECK(g_browser_process->profile_manager());
  g_browser_process->profile_manager()->RemoveObserver(this);
}

void GlicBackgroundModeManager::EnterBackgroundMode() {
  KeepAliveRegistry* const keep_alive_registry =
      KeepAliveRegistry::GetInstance();
  if (!keep_alive_ && keep_alive_registry &&
      !keep_alive_registry->IsShuttingDown()) {
    keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::GLIC_LAUNCHER, KeepAliveRestartOption::ENABLED);
  }

  if (!status_icon_) {
    status_icon_ =
        std::make_unique<GlicStatusIcon>(controller_.get(), status_tray_);
  }
}

void GlicBackgroundModeManager::ExitBackgroundMode() {
  status_icon_.reset();
  keep_alive_.reset();
}

void GlicBackgroundModeManager::EnableLaunchOnStartup(bool should_launch) {
#if BUILDFLAG(IS_WIN)
  if (should_launch) {
    StartupLaunchManager::GetInstance()->RegisterLaunchOnStartup(
        StartupLaunchReason::kGlic);
  } else {
    StartupLaunchManager::GetInstance()->UnregisterLaunchOnStartup(
        StartupLaunchReason::kGlic);
  }
#endif
}

void GlicBackgroundModeManager::RegisterHotkey(ui::Accelerator updated_hotkey) {
  CHECK(!updated_hotkey.IsEmpty());
  auto* const global_accelerator_listener =
      ui::GlobalAcceleratorListener::GetInstance();
  if (global_accelerator_listener) {
    const bool shortcut_handling_suspended =
        global_accelerator_listener->IsShortcutHandlingSuspended();
    // Re-enable shortcut handling to allow the global accelerator listener to
    // register the hotkey.
    global_accelerator_listener->SetShortcutHandlingSuspended(false);
    if (global_accelerator_listener->RegisterAccelerator(updated_hotkey,
                                                         this)) {
      actual_registered_hotkey_ = updated_hotkey;
    }
    global_accelerator_listener->SetShortcutHandlingSuspended(
        shortcut_handling_suspended);
  }
}

void GlicBackgroundModeManager::UnregisterHotkey() {
  auto* const global_accelerator_listener =
      ui::GlobalAcceleratorListener::GetInstance();
  if (global_accelerator_listener && !actual_registered_hotkey_.IsEmpty()) {
    global_accelerator_listener->UnregisterAccelerator(
        actual_registered_hotkey_, this);
  }
  actual_registered_hotkey_ = ui::Accelerator();
}

void GlicBackgroundModeManager::UpdateState() {
  UnregisterHotkey();

  bool background_mode_enabled = enabled_pref_ && IsEnabledInAnyLoadedProfile();
  if (background_mode_enabled) {
    EnterBackgroundMode();
    if (!expected_registered_hotkey_.IsEmpty()) {
      RegisterHotkey(expected_registered_hotkey_);
    }
  } else {
    ExitBackgroundMode();
  }

  if (status_icon_) {
    status_icon_->UpdateHotkey(actual_registered_hotkey_);
  }
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

}  // namespace glic
