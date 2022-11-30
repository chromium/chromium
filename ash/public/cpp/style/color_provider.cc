// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/style/color_provider.h"

#include "base/check_op.h"

namespace ash {

namespace {
ColorProvider* g_instance = nullptr;
}

// static
constexpr float ColorProvider::kBackgroundBlurSigma;

// static
constexpr float ColorProvider::kBackgroundBlurQuality;

// static
ColorProvider* ColorProvider::Get() {
  return g_instance;
}

ColorProvider::ColorProvider() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

ColorProvider::~ColorProvider() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
