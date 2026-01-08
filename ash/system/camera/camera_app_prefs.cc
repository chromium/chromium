// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/camera/camera_app_prefs.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash::camera_app_prefs {

PrefService* GetPrimaryUserPrefService() {
  return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kCameraAppDevToolsOpen, false);
  registry->RegisterBooleanPref(prefs::kCameraAppSkipCancelUploadDialog, false);
}

bool ShouldDevToolsOpen() {
  return GetPrimaryUserPrefService()->GetBoolean(prefs::kCameraAppDevToolsOpen);
}

void SetDevToolsOpenState(bool is_opened) {
  GetPrimaryUserPrefService()->SetBoolean(prefs::kCameraAppDevToolsOpen,
                                          is_opened);
}

ASH_EXPORT bool ShouldSkipCancelUploadDialog() {
  return GetPrimaryUserPrefService()->GetBoolean(
      prefs::kCameraAppSkipCancelUploadDialog);
}

ASH_EXPORT void SetSkipCancelUploadDialog() {
  GetPrimaryUserPrefService()->SetBoolean(
      prefs::kCameraAppSkipCancelUploadDialog, true);
}

}  // namespace ash::camera_app_prefs
