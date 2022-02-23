// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_ASH_COLOR_MIXER_H_
#define ASH_STYLE_ASH_COLOR_MIXER_H_

#include "ui/color/color_provider_manager.h"

namespace ui {
class ColorProvider;
}

namespace ash {

// Adds a color mixer to `provider` that supplies default values for various
// ash/ colors before taking into account any custom themes.
void AddAshColorMixer(ui::ColorProvider* provider,
                      const ui::ColorProviderManager::Key& key);

}  // namespace ash

#endif  // ASH_STYLE_ASH_COLOR_MIXER_H_
