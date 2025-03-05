// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/glic/glic_controller.h"

#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_profile_manager.h"

namespace glic {

GlicController::GlicController() = default;
GlicController::~GlicController() = default;

void GlicController::Toggle(InvocationSource source) {
  ToggleUI(/*prevent_close=*/false, source);
}

void GlicController::Show(InvocationSource source) {
  ToggleUI(/*prevent_close=*/true, source);
}

void GlicController::ToggleUI(bool prevent_close, InvocationSource source) {
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

}  // namespace glic
