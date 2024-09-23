// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_ASH_COLOR_PROVIDER_SOURCE_H_
#define ASH_STYLE_ASH_COLOR_PROVIDER_SOURCE_H_

#include "ash/ash_export.h"
#include "base/scoped_observation.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_source.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

namespace ash {

// A ColorProviderSource used when we need to apply color on an object which is
// not in the view hierarchy and there's no related widget anywhere we can plumb
// across from.
class ASH_EXPORT AshColorProviderSource : public ui::ColorProviderSource,
                                          public ui::NativeThemeObserver {
 public:
  AshColorProviderSource();
  AshColorProviderSource(const AshColorProviderSource&) = delete;
  AshColorProviderSource& operator=(const AshColorProviderSource&) = delete;
  ~AshColorProviderSource() override;

  // ui::ColorProviderSource:
  const ui::ColorProvider* GetColorProvider() const override;

  ui::RendererColorMap GetRendererColorMap(
      ui::ColorProviderKey::ColorMode color_mode,
      ui::ColorProviderKey::ForcedColors forced_colors) const override;

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

 protected:
  // ui::ColorProviderSource:
  ui::ColorProviderKey GetColorProviderKey() const override;

 private:
  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      native_theme_observation_{this};
};

}  // namespace ash

#endif  // ASH_STYLE_ASH_COLOR_PROVIDER_SOURCE_H_
