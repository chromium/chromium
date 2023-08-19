// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ROUNDED_DISPLAY_ROUNDED_DISPLAY_PROVIDER_TEST_API_H_
#define ASH_ROUNDED_DISPLAY_ROUNDED_DISPLAY_PROVIDER_TEST_API_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/rounded_display/rounded_display_provider.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace ash {

class RoundedDisplayProvider;
class RoundedDisplayGutter;

class ASH_EXPORT RoundedDisplayProviderTestApi {
 public:
  explicit RoundedDisplayProviderTestApi(RoundedDisplayProvider* provider);

  RoundedDisplayProviderTestApi(const RoundedDisplayProviderTestApi&) = delete;
  RoundedDisplayProviderTestApi& operator=(
      const RoundedDisplayProviderTestApi&) = delete;

  ~RoundedDisplayProviderTestApi();

  RoundedDisplayProvider::Strategy GetCurrentStrategy() const;

  const gfx::RoundedCornersF GetCurrentPanelRadii() const;

  const aura::Window* GetHostWindow() const;

  std::vector<RoundedDisplayGutter*> GetGutters() const;

 private:
  raw_ptr<RoundedDisplayProvider, DanglingUntriaged> provider_;
};

}  // namespace ash

#endif  // ASH_ROUNDED_DISPLAY_ROUNDED_DISPLAY_PROVIDER_TEST_API_H_
