// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rounded_display/rounded_display_provider_test_api.h"

#include "ash/rounded_display/rounded_display_gutter.h"

namespace ash {

RoundedDisplayProviderTestApi::RoundedDisplayProviderTestApi(
    RoundedDisplayProvider* provider)
    : provider_(provider) {}

RoundedDisplayProviderTestApi::~RoundedDisplayProviderTestApi() = default;

const gfx::RoundedCornersF RoundedDisplayProviderTestApi::GetCurrentPanelRadii()
    const {
  return provider_->current_panel_radii_;
}

RoundedDisplayProvider::Strategy
RoundedDisplayProviderTestApi::GetCurrentStrategy() const {
  return provider_->strategy_;
}

const aura::Window* RoundedDisplayProviderTestApi::GetHostWindow() const {
  return provider_->host_window_.get();
}

std::vector<RoundedDisplayGutter*> RoundedDisplayProviderTestApi::GetGutters()
    const {
  std::vector<RoundedDisplayGutter*> gutters;
  provider_->GetGuttersInDrawOrder(gutters);
  return gutters;
}

}  // namespace ash
