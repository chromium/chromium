// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/glic/glic_controller.h"

#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/common/chrome_features.h"

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
  if (GlicEnabling::IsMultiInstanceEnabled()) {
    glic_keyed_service->ToggleUI(nullptr, /*prevent_close=*/false,
                                 mojom::InvocationSource::kOsButton);
  } else {
    glic_keyed_service->CloseAndShutdown();
  }
}

bool GlicController::IsShowing() const {
  GlicKeyedService* glic_keyed_service =
      glic::GlicProfileManager::GetInstance()->GetLastActiveGlic();
  return glic_keyed_service && glic_keyed_service->IsWindowShowing();
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

}  // namespace glic
