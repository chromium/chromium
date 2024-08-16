// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_UTIL_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_UTIL_H_

#include <string>

#include "ash/ash_export.h"

class PrefRegistrySimple;

namespace aura {
class Window;
}

namespace ash {

class Desk;
class DeskBarViewBase;
class DeskTemplate;
class SavedDeskDialogController;
class SavedDeskPresenter;

struct SaveDeskOptionStatus {
  bool enabled;
  int tooltip_id;
};

namespace saved_desk_util {
// Registers the per-profile preferences for whether desks templates are
// enabled.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

ASH_EXPORT bool AreDesksTemplatesEnabled();

ASH_EXPORT bool ShouldShowSavedDesksOptions();

ASH_EXPORT bool ShouldShowSavedDesksOptionsForDesk(Desk* desk,
                                                   DeskBarViewBase* bar_view);

// Will return null if overview mode is not active.
ASH_EXPORT SavedDeskDialogController* GetSavedDeskDialogController();

// Will return null if overview mode is not active.
ASH_EXPORT SavedDeskPresenter* GetSavedDeskPresenter();

// Returns the app ID of the window, if present. Returns an empty string
// otherwise.
ASH_EXPORT std::string GetAppId(aura::Window* window);

// Returns true if `window` was launched from an admin template and should be on
// top relative to other desk templates windows.
bool IsWindowOnTopForTemplate(aura::Window* window);

// This function updates the activation indices of all the windows in a
// template so that windows launched from it will stack in the order they are
// defined, while also stacking on top of any existing windows.
ASH_EXPORT void UpdateTemplateActivationIndices(DeskTemplate& saved_desk);

// This function updates the activation indices of all the windows in a
// template so that windows launched from it will stack in the order that they
// were stacked upon saving as a template, while also stacking on top of any
// existing windows.
ASH_EXPORT void UpdateTemplateActivationIndicesRelativeOrder(
    DeskTemplate& saved_desk);

}  // namespace saved_desk_util
}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_SAVED_DESK_UTIL_H_
