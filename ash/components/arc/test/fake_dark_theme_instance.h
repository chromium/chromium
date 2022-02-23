// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_DARK_THEME_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_DARK_THEME_INSTANCE_H_

#include "ash/components/arc/mojom/dark_theme.mojom.h"

namespace arc {

class FakeDarkThemeInstance : public mojom::DarkThemeInstance {
 public:
  FakeDarkThemeInstance();
  FakeDarkThemeInstance(const FakeDarkThemeInstance&) = delete;
  FakeDarkThemeInstance& operator=(const FakeDarkThemeInstance&) = delete;
  ~FakeDarkThemeInstance() override;

  bool dark_theme_status() const { return dark_theme_status_; }

  // mojom::DarkThemeInstance overrides:
  void DarkThemeStatus(bool darkThemeStatus) override;

 private:
  bool dark_theme_status_ = false;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_DARK_THEME_INSTANCE_H_
