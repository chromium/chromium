// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"

#include "build/build_config.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller_impl.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/public/glic_keyed_service.h"
#endif

namespace glic {

DEFINE_USER_DATA(GlicNudgeController);

GlicNudgeController::~GlicNudgeController() = default;

// static
std::unique_ptr<GlicNudgeController> GlicNudgeController::CreateFor(
    BrowserWindowInterface* browser) {
  return std::make_unique<GlicNudgeControllerImpl>(browser);
}

// static
GlicNudgeController* GlicNudgeController::From(
    BrowserWindowInterface* browser) {
  if (!browser) {
    return nullptr;
  }
  auto* controller =
      GlicNudgeController::Get(browser->GetUnownedUserDataHost());
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/484037810): Once a window features object is supported on
  // Android, this lazy creation fallback via GlicKeyedService can be removed,
  // and we can rely solely on the UnownedUserDataHost lookup.
  if (!controller) {
    auto* service = GlicKeyedService::Get(browser->GetProfile());
    if (service) {
      controller = service->GetOrCreateNudgeController(browser);
    }
  }
#endif
  return controller;
}

}  // namespace glic
