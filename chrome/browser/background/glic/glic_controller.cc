// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/glic/glic_controller.h"

#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"

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
  GlicProfileManager* profile_manager = GlicProfileManager::GetInstance();
  if (profile_manager && profile_manager->GetProfileForLaunch()) {
    Profile* profile = profile_manager->GetProfileForLaunch();
    if (auto* service = GlicKeyedServiceFactory::GetGlicKeyedService(profile)) {
      service->instance_coordinator().Close({});
    }
  }
}

void GlicController::ToggleUI(bool prevent_close,
                              mojom::InvocationSource source) {
  Profile* profile =
      glic::GlicProfileManager::GetInstance()->GetProfileForLaunch();
  if (!profile) {
    return;
  }

  GlicKeyedService* glic_keyed_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  glic_keyed_service->ToggleUI(nullptr, prevent_close, source);
}

}  // namespace glic
