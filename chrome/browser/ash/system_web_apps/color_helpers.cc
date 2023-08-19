// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/color_helpers.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/native_theme/native_theme.h"

SkColor ash::GetSystemThemeColor() {
  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  auto* color_provider = ui::ColorProviderManager::Get().GetColorProviderFor(
      native_theme->GetColorProviderKey(nullptr));
  return color_provider->GetColor(cros_tokens::kCrosSysHeader);
}

SkColor ash::GetSystemBackgroundColor() {
  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  auto* color_provider = ui::ColorProviderManager::Get().GetColorProviderFor(
      native_theme->GetColorProviderKey(nullptr));
  return color_provider->GetColor(cros_tokens::kCrosSysAppBase);
}
