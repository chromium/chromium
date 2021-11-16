// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_dark_theme_instance.h"

#include "base/callback_helpers.h"

namespace arc {

FakeDarkThemeInstance::FakeDarkThemeInstance() = default;

FakeDarkThemeInstance::~FakeDarkThemeInstance() = default;

void FakeDarkThemeInstance::DarkThemeStatus(bool darkThemeStatus) {
  dark_theme_status_ = darkThemeStatus;
}

}  // namespace arc
