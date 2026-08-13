// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEIC_GEIC_BUTTON_H_
#define CHROME_BROWSER_GEIC_GEIC_BUTTON_H_

#include <string>

#include "chrome/browser/ui/views/glic/glic_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_nudge_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

class BrowserWindowInterface;

namespace geic {

class GeicButton : public glic::GlicButton<TabStripNudgeButton> {
  METADATA_HEADER(GeicButton, TabStripNudgeButton)

 public:
  static std::unique_ptr<GeicButton> Create(
      BrowserWindowInterface* browser_window_interface);

  GeicButton(BrowserWindowInterface* browser_window_interface,
             const std::u16string& tooltip,
             PressedCallback pressed_callback);

  GeicButton(const GeicButton&) = delete;
  GeicButton& operator=(const GeicButton&) = delete;
  ~GeicButton() override;

  // TabStripNudgeButton:
  bool GetIsShowingNudge() const override;
  gfx::SlideAnimation* GetExpansionAnimationForTesting() override;

  // glic::GlicButton:
  void ResetSplitButtonCornerStyling() override;
  void SetLabelMargins() override;

 private:
  ui::ColorId GetCustomThemeForegroundId() const override;
  ui::ColorId GetCustomThemeBackgroundActiveId() const override;
  ui::ColorId GetCustomThemeBackgroundInactiveId() const override;
  ui::ColorId GetCustomThemeForegroundActiveId() const override;
  ui::ColorId GetCustomThemeForegroundInactiveId() const override;

  void OnLabelVisibilityChanged() override;
  float GetWidthFactor() const override;
};

}  // namespace geic

#endif  // CHROME_BROWSER_GEIC_GEIC_BUTTON_H_
