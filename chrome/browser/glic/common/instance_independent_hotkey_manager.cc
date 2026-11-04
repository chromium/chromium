// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/instance_independent_hotkey_manager.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/common/application_hotkey_delegate.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_feature.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/widget/browser_conditions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "components/prefs/pref_service.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/glic_selection_observer.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/selection/selection_overlay.mojom.h"
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/gfx/geometry/rect.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/platform_util.h"
#endif

namespace glic {

namespace {
// Whether `profile`'s provider supports OS-level entry points, including the
// OS-global hotkey registered by `GlicBackgroundModeManager`. When it doesn't
// (e.g. Gemini Enterprise), the hotkey is only handled here, while a Chrome
// window is focused.
bool SupportsOsEntryPoints(Profile* profile) {
  return GetGlicProviderSpec(profile).SupportsFeature(
      GlicFeature::kOsEntryPoints);
}

// Whether `profile`'s panel toggle hotkey is scoped globally, i.e. registered
// as an OS-global hotkey by `GlicBackgroundModeManager` rather than handled by
// `InstanceIndependentHotkeyManager` inside Chrome. This is the case when the
// local scope feature is disabled or the user selected Global scope, unless
// `profile`'s provider doesn't support OS-level entry points.
bool IsPanelToggleHotkeyGlobal(Profile* profile) {
  if (!SupportsOsEntryPoints(profile)) {
    return false;
  }
  if (!base::FeatureList::IsEnabled(features::kGlicHotkeyLocalScope)) {
    return true;
  }
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  PrefService* const local_state = g_browser_process->local_state();
  return !local_state ||
         local_state->GetBoolean(prefs::kGlicHotkeyGlobalScopeEnabled);
#endif
}

#if !BUILDFLAG(IS_ANDROID)
BrowserWindowInterface* GetLastActiveBrowser(Profile* profile) {
  auto* const collection = ProfileBrowserCollection::GetForProfile(profile);
  return collection ? collection->GetLastActiveBrowser() : nullptr;
}
#endif
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
    consent_subscription_ =
        enabling->RegisterOnConsentChanged(base::BindRepeating(
            &InstanceIndependentHotkeyManager::UpdateHotkeyRegistration,
            base::Unretained(this)));
    enabled_subscription_ =
        enabling->RegisterAllowedChanged(base::BindRepeating(
            &InstanceIndependentHotkeyManager::UpdateHotkeyRegistration,
            base::Unretained(this)));
  }
  UpdateHotkeyRegistration();
}

InstanceIndependentHotkeyManager::~InstanceIndependentHotkeyManager() = default;

void InstanceIndependentHotkeyManager::UpdateHotkeyRegistration() {
  const bool registration_allowed =
      !SupportsOsEntryPoints(profile_) ||
      base::FeatureList::IsEnabled(features::kGlicHotkeyLocalScope) ||
      GlicLauncherConfiguration::IsLauncherIconEnabled();
  if (registration_allowed &&
      GlicEnabling::IsEnabledAndConsentForProfile(profile_)) {
    if (!hotkey_manager_) {
      std::vector<LocalHotkeyManager::Command> supported_commands = {
          LocalHotkeyManager::Command::kPanelToggle,
      };
      if (GetGlicProviderSpec(profile_).SupportsFeature(
              GlicFeature::kCaptureRegion)) {
        supported_commands.push_back(
            LocalHotkeyManager::Command::kCaptureRegion);
      }
      hotkey_manager_ = std::make_unique<LocalHotkeyManager>(
          std::make_unique<ApplicationScopedRegistrationDelegate>(profile_),
          this, supported_commands);
      hotkey_manager_->InitializeAccelerators();
    }
  } else {
    hotkey_manager_.reset();
  }
}

#if !BUILDFLAG(IS_ANDROID)
void InstanceIndependentHotkeyManager::RequestCaptureRegion() {
  if (!GetGlicProviderSpec(profile_).SupportsFeature(
          GlicFeature::kCaptureRegion)) {
    return;
  }
  BrowserWindowInterface* const bwi = GetLastActiveBrowser(profile_);
  // bwi is guaranteed to be valid and belong to profile_ because of
  // CanHandleAccelerators.
  CHECK(bwi);
  CHECK_EQ(bwi->GetProfile(), profile_);
#if BUILDFLAG(IS_CHROMEOS)
  if (platform_util::IsBrowserLockedFullscreen(bwi)) {
    return;
  }
#endif
  auto* active_tab = bwi->GetActiveTabInterface();
  if (!active_tab) {
    return;
  }
  if (!base::FeatureList::IsEnabled(features::kGlicSelectionOverlayPromptBox) ||
      GlicSidePanelCoordinator::IsShowing(active_tab)) {
    GlicInvokeOptions options(
        Target(*active_tab),
        glic::mojom::InvocationSource::kCaptureRegionHotkey);
    options.wait_for_panel_open = true;
    coordinator_->Invoke(std::move(options));
    return;
  }

  auto* controller =
      SelectionOverlayController::FromTabWebContents(active_tab->GetContents());
  if (!controller) {
    return;
  }
  if (auto* selection_observer = GlicSelectionObserver::From(active_tab)) {
    if (std::optional<gfx::Rect> bounds =
            selection_observer->GetCurrentSelectionBounds()) {
      selection_observer->DismissUI(
          GlicSelectionObserver::DismissReason::kActionTaken);
      controller->ShowWithSelection(
          selection_observer->GetSelectedFrame(), *bounds,
          selection::InteractionOptions::New(
              /*hide_handles=*/true, /*disable_multi_select=*/true,
              /*skip_intro_animation=*/true),
          ::selection::EntryPoint::kKeyboardShortcut);
      return;
    }
  }
  controller->Show(/*options=*/nullptr,
                   ::selection::EntryPoint::kKeyboardShortcut);
}
#endif

bool InstanceIndependentHotkeyManager::AcceleratorPressed(
    LocalHotkeyManager::Command command) {
  switch (command) {
    case LocalHotkeyManager::Command::kPanelToggle: {
      // If the hotkey is scoped globally, let this local manager pass through
      // (return false) to avoid handling it locally inside Chrome. Profiles
      // whose provider doesn't support OS-level entry points never register
      // the global hotkey, so it is always handled locally for them. If
      // another profile has the global hotkey registered, the OS delivers the
      // key to it first and it launches that other profile.
      if (IsPanelToggleHotkeyGlobal(profile_)) {
        return false;
      }
      coordinator_->Toggle(GetActiveGlicEligibleBrowser(profile_),
                           /*prevent_close=*/false,
                           mojom::InvocationSource::kOsHotkey);
      return true;
    }
#if !BUILDFLAG(IS_ANDROID)
    case LocalHotkeyManager::Command::kCaptureRegion:
      if (!GetGlicProviderSpec(profile_).SupportsFeature(
              GlicFeature::kCaptureRegion)) {
        return false;
      }
      RequestCaptureRegion();
      return true;
#endif
    default:
      return false;
  }
}

bool InstanceIndependentHotkeyManager::CanHandleAccelerators() const {
  if (!hotkey_manager_ ||
      !GlicEnabling::IsEnabledAndConsentForProfile(profile_)) {
    return false;
  }
#if BUILDFLAG(IS_CHROMEOS)
  BrowserWindowInterface* const bwi = GetLastActiveBrowser(profile_);
  if (bwi && platform_util::IsBrowserLockedFullscreen(bwi)) {
    return false;
  }
#endif
  return true;
}

}  // namespace glic
