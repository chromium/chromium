// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_settings.h"

#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/class_property.h"
#include "ui/display/types/display_constants.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ash::RootWindowSettings*)

namespace ash {

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(RootWindowSettings,
                                   kRootWindowSettingsKey,
                                   NULL)

RootWindowSettings::RootWindowSettings()
    : display_id(display::kInvalidDisplayId), controller(NULL) {}

RootWindowSettings* InitRootWindowSettings(aura::Window* root) {
  RootWindowSettings* settings = new RootWindowSettings();
  root->SetProperty(kRootWindowSettingsKey, settings);
  return settings;
}

RootWindowSettings* GetRootWindowSettings(aura::Window* root) {
  return root->GetProperty(kRootWindowSettingsKey);
}

const RootWindowSettings* GetRootWindowSettings(const aura::Window* root) {
  return root->GetProperty(kRootWindowSettingsKey);
}

}  // namespace ash
