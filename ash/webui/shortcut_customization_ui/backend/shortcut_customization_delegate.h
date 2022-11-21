// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_SHORTCUT_CUSTOMIZATION_DELEGATE_H_
#define ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_SHORTCUT_CUSTOMIZATION_DELEGATE_H_

#include "components/prefs/pref_service.h"

namespace ash::shortcut_ui {

// A delegate which exposes browser functionality from //chrome to the Shortcut
// Customization UI.
class ShortcutCustomizationDelegate {
 public:
  virtual ~ShortcutCustomizationDelegate() = default;

  // Get the pref service.
  virtual PrefService* GetPrefService() = 0;
};

}  // namespace ash::shortcut_ui

#endif  // ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_BACKEND_SHORTCUT_CUSTOMIZATION_DELEGATE_H_