// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/glic/glic_controller.h"

#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

GlicController::GlicController() = default;
GlicController::~GlicController() = default;

void GlicController::Toggle(mojom::InvocationSource source) {
  ToggleUI(/*prevent_close=*/false, source);
}

void GlicController::Show(mojom::InvocationSource source) {
  ToggleUI(/*prevent_close=*/true, source);
}

void GlicController::Close() {
  GlicKeyedService* glic_keyed_service =
      glic::GlicProfileManager::GetInstance()->GetLastActiveGlic();
  if (!glic_keyed_service) {
    return;
  }
  glic_keyed_service->ToggleUI(nullptr, /*prevent_close=*/false,
                               mojom::InvocationSource::kOsButton);
}

bool GlicController::IsShowing() const {
  GlicKeyedService* glic_keyed_service =
      glic::GlicProfileManager::GetInstance()->GetLastActiveGlic();
  return glic_keyed_service &&
         glic_keyed_service->instance_coordinator().IsAnyPanelShowing();
}

void GlicController::ToggleUI(bool prevent_close,
                              mojom::InvocationSource source) {
  Profile* profile =
      glic::GlicProfileManager::GetInstance()->GetProfileForLaunch();
  if (!profile) {
    // TODO(crbug.com/380095872): If there are no eligible profiles, show the
    // profile picker to choose a profile in which to enter the first-run
    // experience.
    return;
  }

  GlicKeyedService* glic_keyed_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  glic_keyed_service->ToggleUI(nullptr, prevent_close, source);
}

void GlicController::RequestCaptureRegion() {
  BrowserWindowInterface* const bwi =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  if (!bwi) {
    return;
  }
  GlicKeyedService* glic_keyed_service =
      GlicKeyedService::Get(bwi->GetProfile());
  if (!glic_keyed_service) {
    return;
  }
  GlicInvokeOptions options(
      glic::mojom::InvocationSource::kCaptureRegionHotkey);
  options.wait_for_panel_open = true;
  options.target = Target(bwi->GetActiveTabInterface());
  glic_keyed_service->Invoke(std::move(options));
}

}  // namespace glic
