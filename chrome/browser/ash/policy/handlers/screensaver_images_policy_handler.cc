// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/screensaver_images_policy_handler.h"

#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/shell.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace policy {

namespace {
ScreensaverImagesPolicyHandler* g_screensaver_images_policy_handler_instance =
    nullptr;
}

// static
ScreensaverImagesPolicyHandler*
ScreensaverImagesPolicyHandler::GetScreensaverImagesPolicyHandlerInstance() {
  return g_screensaver_images_policy_handler_instance;
}

ScreensaverImagesPolicyHandler::ScreensaverImagesPolicyHandler() {
  DCHECK(!g_screensaver_images_policy_handler_instance);
  g_screensaver_images_policy_handler_instance = this;
}

ScreensaverImagesPolicyHandler::~ScreensaverImagesPolicyHandler() {
  DCHECK(g_screensaver_images_policy_handler_instance);
  g_screensaver_images_policy_handler_instance = nullptr;
}

// static
void ScreensaverImagesPolicyHandler::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(
      ash::ambient::prefs::kAmbientModeManagedScreensaverImages);
}

void ScreensaverImagesPolicyHandler::
    OnAmbientModeManagedScreensaverImagesPrefChanged() {
  PrefService* pref_service =
      ash::Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  if (!pref_service) {
    return;
  }

  // TODO(b/271093572): Read the value from the pref and try to download
}

void ScreensaverImagesPolicyHandler::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  if (pref_service !=
      ash::Shell::Get()->session_controller()->GetPrimaryUserPrefService()) {
    return;
  }

  if (pref_change_registrar_) {
    return;
  }

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);

  pref_change_registrar_->Add(
      ash::ambient::prefs::kAmbientModeManagedScreensaverImages,
      base::BindRepeating(&ScreensaverImagesPolicyHandler::
                              OnAmbientModeManagedScreensaverImagesPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  OnAmbientModeManagedScreensaverImagesPrefChanged();
}

}  // namespace policy
