// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_ASH_COLOR_MIXER_H_
#define ASH_STYLE_ASH_COLOR_MIXER_H_

#include "ash/ash_export.h"
#include "ui/color/color_provider_manager.h"

namespace ui {
class ColorProvider;
}  // namespace ui

namespace ash {

// Adds a color mixer with colors generated from ui/chromeos/styles/*.json5.
ASH_EXPORT void AddCrosStylesColorMixer(
    ui::ColorProvider* provider,
    const ui::ColorProviderManager::Key& key);

// Adds a color mixer to `provider` that supplies default values for various
// ash/ colors before taking into account any custom themes.
ASH_EXPORT void AddAshColorMixer(ui::ColorProvider* provider,
                                 const ui::ColorProviderManager::Key& key);

}  // namespace ash

#endif  // ASH_STYLE_ASH_COLOR_MIXER_H_
