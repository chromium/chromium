// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/instance_independent_hotkey_manager.h"

#include <array>

#include "base/feature_list.h"
#include "build/build_config.h"
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
    Profile* profile)
    : coordinator_(coordinator), profile_(profile) {
  hotkey_manager_ = std::make_unique<LocalHotkeyManager>(
      std::make_unique<ApplicationScopedRegistrationDelegate>(profile), this,
      kSupportedCommands);
  hotkey_manager_->InitializeAccelerators();
}

InstanceIndependentHotkeyManager::~InstanceIndependentHotkeyManager() = default;

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
      // If the hotkey is scoped globally (i.e. local scope is disabled),
      // it is handled globally by GlicBackgroundModeManager. Let this local
      // manager pass through to prevent duplicate triggering inside Chrome.
      if (!base::FeatureList::IsEnabled(features::kGlicHotkeyLocalScope)) {
        return false;
      }
      PrefService* local_state = g_browser_process->local_state();
      if (!local_state ||
          !local_state->GetBoolean(prefs::kGlicLauncherEnabled)) {
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
  return GlicEnabling::IsEnabledAndConsentForProfile(profile_);
}

}  // namespace glic
