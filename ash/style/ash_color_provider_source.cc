// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/ash_color_provider_source.h"

#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"

namespace ash {

AshColorProviderSource::AshColorProviderSource() {
  native_theme_observation_.Observe(ui::NativeTheme::GetInstanceForNativeUi());
}

AshColorProviderSource::~AshColorProviderSource() = default;

const ui::ColorProvider* AshColorProviderSource::GetColorProvider() const {
  return ui::ColorProviderManager::Get().GetColorProviderFor(
      GetColorProviderKey());
}

ui::RendererColorMap AshColorProviderSource::GetRendererColorMap(
    ui::ColorProviderKey::ColorMode color_mode,
    ui::ColorProviderKey::ForcedColors forced_colors) const {
  auto key = GetColorProviderKey();
  key.color_mode = color_mode;
  key.forced_colors = forced_colors;
  ui::ColorProvider* color_provider =
      ui::ColorProviderManager::Get().GetColorProviderFor(key);
  CHECK(color_provider);
  return ui::CreateRendererColorMap(*color_provider);
}

void AshColorProviderSource::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  DCHECK(native_theme_observation_.IsObservingSource(observed_theme));
  NotifyColorProviderChanged();
}

ui::ColorProviderKey AshColorProviderSource::GetColorProviderKey() const {
  return ui::NativeTheme::GetInstanceForNativeUi()->GetColorProviderKey(
      nullptr);
}

}  // namespace ash
