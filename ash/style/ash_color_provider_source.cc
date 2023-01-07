// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/ash_color_provider_source.h"

#include "ui/color/color_provider.h"

namespace ash {

AshColorProviderSource::AshColorProviderSource() {
  native_theme_observation_.Observe(ui::NativeTheme::GetInstanceForNativeUi());
}

AshColorProviderSource::~AshColorProviderSource() = default;

const ui::ColorProvider* AshColorProviderSource::GetColorProvider() const {
  return ui::ColorProviderManager::Get().GetColorProviderFor(
      GetColorProviderKey());
}

void AshColorProviderSource::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  DCHECK(native_theme_observation_.IsObservingSource(observed_theme));
  NotifyColorProviderChanged();
}

ui::ColorProviderManager::Key AshColorProviderSource::GetColorProviderKey()
    const {
  return ui::NativeTheme::GetInstanceForNativeUi()->GetColorProviderKey(
      nullptr);
}

}  // namespace ash
