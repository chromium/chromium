// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_system_ui_instance.h"

#include "base/functional/callback_helpers.h"

namespace arc {

FakeSystemUiInstance::FakeSystemUiInstance() = default;

FakeSystemUiInstance::~FakeSystemUiInstance() = default;

void FakeSystemUiInstance::SetDarkThemeStatus(bool darkThemeStatus) {
  dark_theme_status_ = darkThemeStatus;
}

void FakeSystemUiInstance::SetOverlayColor(uint32_t sourceColor,
                                           mojom::ThemeStyleType themeStyle) {
  source_color_ = sourceColor;
  theme_style_ = themeStyle;
}

}  // namespace arc
