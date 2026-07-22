// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/instance_independent_hotkey_manager.h"

#include <array>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/common/application_hotkey_delegate.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/widget/browser_conditions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "components/prefs/pref_service.h"

namespace glic {

namespace {
constexpr LocalHotkeyManager::Command kSupportedCommands[] = {
    LocalHotkeyManager::Command::kPanelToggle,
    LocalHotkeyManager::Command::kCaptureRegion,
};
}  // namespace

InstanceIndependentHotkeyManager::InstanceIndependentHotkeyManager(
    GlicInstanceCoordinator* coordinator,
    Profile* profile,
    GlicEnabling* enabling)
    : coordinator_(coordinator), profile_(profile) {
  // g_browser_process->local_state() can be null in some test environments.
  if (PrefService* local_state = g_browser_process->local_state()) {
    pref_registrar_.Init(local_state);
    pref_registrar_.Add(
        prefs::kGlicLauncherEnabled,
        base::BindRepeating(
            &InstanceIndependentHotkeyManager::UpdateHotkeyRegistration,
            base::Unretained(this)));
  }
  // `enabling` can be null in some test environments.
  if (enabling) {
    consent_subscription_ = enabling->RegisterOnConsentChanged(
        base::BindRepeating(
            &InstanceIndependentHotkeyManager::UpdateHotkeyRegistration,
            base::Unretained(this)));
    enabled_subscription_ = enabling->RegisterAllowedChanged(
        base::BindRepeating(
            &InstanceIndependentHotkeyManager::UpdateHotkeyRegistration,
            base::Unretained(this)));
  }
  UpdateHotkeyRegistration();
}

InstanceIndependentHotkeyManager::~InstanceIndependentHotkeyManager() = default;

void InstanceIndependentHotkeyManager::UpdateHotkeyRegistration() {
  if (GlicLauncherConfiguration::IsEnabled() &&
      GlicEnabling::IsEnabledAndConsentForProfile(profile_)) {
    if (!hotkey_manager_) {
      hotkey_manager_ = std::make_unique<LocalHotkeyManager>(
          std::make_unique<ApplicationScopedRegistrationDelegate>(profile_), this,
          kSupportedCommands);
      hotkey_manager_->InitializeAccelerators();
    }
  } else {
    hotkey_manager_.reset();
  }
}

#if !BUILDFLAG(IS_ANDROID)
void InstanceIndependentHotkeyManager::RequestCaptureRegion() {
  BrowserWindowInterface* const bwi =
      GlobalBrowserCollection::GetInstance()->GetActiveBrowser();
  // bwi is guaranteed to be valid and belong to profile_ because of
  // CanHandleAccelerators.
  CHECK(bwi);
  CHECK_EQ(bwi->GetProfile(), profile_);
  auto* active_tab = bwi->GetActiveTabInterface();
  if (!active_tab) {
    return;
  }
  GlicInvokeOptions options(
      Target(*active_tab), glic::mojom::InvocationSource::kCaptureRegionHotkey);
  options.wait_for_panel_open = true;
  coordinator_->Invoke(std::move(options));
}
#endif

bool InstanceIndependentHotkeyManager::AcceleratorPressed(
    LocalHotkeyManager::Command command) {
  switch (command) {
    case LocalHotkeyManager::Command::kPanelToggle: {
      // If the hotkey is scoped globally (e.g., because the local scope
      // feature is disabled or the user selected Global scope), let this local
      // manager pass through (return false) to avoid handling it locally inside
      // Chrome.
      PrefService* const local_state = g_browser_process->local_state();
      if (!base::FeatureList::IsEnabled(features::kGlicHotkeyLocalScope) ||
          !local_state
#if !BUILDFLAG(IS_ANDROID)
          || local_state->GetBoolean(prefs::kGlicHotkeyGlobalScopeEnabled)
#endif
      ) {
        return false;
      }
      coordinator_->Toggle(GetActiveGlicEligibleBrowser(profile_),
                           /*prevent_close=*/false,
                           mojom::InvocationSource::kOsHotkey);
      return true;
    }
#if !BUILDFLAG(IS_ANDROID)
    case LocalHotkeyManager::Command::kCaptureRegion:
      RequestCaptureRegion();
      return true;
#endif
    default:
      return false;
  }
}

bool InstanceIndependentHotkeyManager::CanHandleAccelerators() const {
  return hotkey_manager_ && GlicEnabling::IsEnabledAndConsentForProfile(profile_);
}

}  // namespace glic
