// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/native_theme_service_ash.h"

#include "chromeos/constants/chromeos_features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/scheme_variant.mojom.h"
#include "ui/native_theme/native_theme.h"

namespace crosapi {
namespace {

color::mojom::SchemeVariant VariantToScheme(
    ui::ColorProviderKey::SchemeVariant scheme) {
  switch (scheme) {
    case ui::ColorProviderKey::SchemeVariant::kTonalSpot:
      return color::mojom::SchemeVariant::kTonalSpot;
    case ui::ColorProviderKey::SchemeVariant::kNeutral:
      return color::mojom::SchemeVariant::kNeutral;
    case ui::ColorProviderKey::SchemeVariant::kVibrant:
      return color::mojom::SchemeVariant::kVibrant;
    case ui::ColorProviderKey::SchemeVariant::kExpressive:
      return color::mojom::SchemeVariant::kExpressive;
  }
  // not reached
}

}  // namespace

/******** NativeThemeServiceAsh::Dispatcher ********/

NativeThemeServiceAsh::Dispatcher::Dispatcher() {
  ui::NativeTheme::GetInstanceForNativeUi()->AddObserver(this);
}

NativeThemeServiceAsh::Dispatcher::~Dispatcher() {
  ui::NativeTheme::GetInstanceForNativeUi()->RemoveObserver(this);
}

void NativeThemeServiceAsh::Dispatcher::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  DCHECK_EQ(ui::NativeTheme::GetInstanceForNativeUi(), observed_theme);

  mojom::NativeThemeInfoPtr info = NativeThemeServiceAsh::GetNativeThemeInfo();
  for (auto& observer : observers_) {
    mojom::NativeThemeInfoPtr info_copy = info->Clone();
    observer->OnNativeThemeInfoChanged(std::move(info_copy));
  }
}

/******** NativeThemeServiceAsh ********/

// static
mojom::NativeThemeInfoPtr NativeThemeServiceAsh::GetNativeThemeInfo() {
  auto info = mojom::NativeThemeInfo::New();
  const ui::NativeTheme* theme = ui::NativeTheme::GetInstanceForNativeUi();
  info->dark_mode = theme->ShouldUseDarkColors();
  info->caret_blink_interval = theme->GetCaretBlinkInterval();

  std::optional<ui::ColorProviderKey::SchemeVariant> scheme =
      theme->scheme_variant();
  if (scheme) {
    info->scheme_variant = VariantToScheme(*scheme);

    // Only set seed color if we also have a `scheme`. Color palette generation
    // is more predictable this way.
    std::optional<SkColor> user_color = theme->user_color();
    if (user_color.has_value()) {
      info->seed_color = *user_color;
    }
  }

  return info;
}

NativeThemeServiceAsh::NativeThemeServiceAsh() = default;

NativeThemeServiceAsh::~NativeThemeServiceAsh() = default;

void NativeThemeServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::NativeThemeService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void NativeThemeServiceAsh::AddNativeThemeInfoObserver(
    mojo::PendingRemote<mojom::NativeThemeInfoObserver> observer) {
  // Fire the observer with the initial value.
  mojo::Remote<mojom::NativeThemeInfoObserver> remote(std::move(observer));
  remote->OnNativeThemeInfoChanged(GetNativeThemeInfo());

  dispatcher_.observers_.Add(std::move(remote));
}

}  // namespace crosapi
