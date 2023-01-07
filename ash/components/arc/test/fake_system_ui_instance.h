// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_SYSTEM_UI_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_SYSTEM_UI_INSTANCE_H_

#include "ash/components/arc/mojom/system_ui.mojom-shared.h"
#include "ash/components/arc/mojom/system_ui.mojom.h"

namespace arc {

class FakeSystemUiInstance : public mojom::SystemUiInstance {
 public:
  FakeSystemUiInstance();
  FakeSystemUiInstance(const FakeSystemUiInstance&) = delete;
  FakeSystemUiInstance& operator=(const FakeSystemUiInstance&) = delete;
  ~FakeSystemUiInstance() override;

  bool dark_theme_status() const { return dark_theme_status_; }

  uint32_t source_color() const { return source_color_; }

  mojom::ThemeStyleType theme_style() const { return theme_style_; }

  // mojom::SystemUiInstance override:
  void SetDarkThemeStatus(bool darkThemeStatus) override;

  // mojom::SystemUiInstance override:
  void SetOverlayColor(uint32_t sourceColor,
                       mojom::ThemeStyleType themeStyle) override;

 private:
  bool dark_theme_status_ = false;
  uint32_t source_color_ = 0;
  mojom::ThemeStyleType theme_style_ = mojom::ThemeStyleType::TONAL_SPOT;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_SYSTEM_UI_INSTANCE_H_
