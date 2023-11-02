// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_STYLE_COLOR_MODE_OBSERVER_H_
#define ASH_PUBLIC_CPP_STYLE_COLOR_MODE_OBSERVER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"

namespace ash {

// Class to observe the current color mode, which can be changed through the
// "Dark theme" feature pod button inside quick settings, personalization hub
// etc. Note, please override views::View::OnThemeChanged() function to update
// colors on color mode changes directly instead of overriding
// OnColorModeChanged() here if possible.
class ASH_PUBLIC_EXPORT ColorModeObserver : public base::CheckedObserver {
 public:
  // Called when the color mode changes.
  virtual void OnColorModeChanged(bool dark_mode_enabled) {}

 protected:
  ~ColorModeObserver() override = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_STYLE_COLOR_MODE_OBSERVER_H_
