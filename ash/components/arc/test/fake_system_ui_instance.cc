// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_system_ui_instance.h"

#include "base/callback_helpers.h"

namespace arc {

FakeSystemUIInstance::FakeSystemUIInstance() = default;

FakeSystemUIInstance::~FakeSystemUIInstance() = default;

void FakeSystemUIInstance::SetDarkThemeStatus(bool darkThemeStatus) {
  dark_theme_status_ = darkThemeStatus;
}

void FakeSystemUIInstance::SetOverlayColor(uint32_t sourceColor,
                                           mojom::ThemeStyleType themeStyle) {
  source_color_ = sourceColor;
  theme_style_ = themeStyle;
}

}  // namespace arc
