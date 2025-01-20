// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/launcher/glic_controller.h"

#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_profile_manager.h"

GlicController::GlicController() = default;
GlicController::~GlicController() = default;

void GlicController::Show() {
  Profile* profile =
      glic::GlicProfileManager::GetInstance()->GetProfileForLaunch();
  if (!profile) {
    // TODO(crbug.com/380095872): If there are no eligible profiles, show the
    // profile picker to choose a profile in which to enter the first-run
    // experience.
    return;
  }

  glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile)->LaunchUI(
      nullptr);
}

void GlicController::Hide() {
  glic::GlicProfileManager::GetInstance()->CloseGlicWindow();
}
