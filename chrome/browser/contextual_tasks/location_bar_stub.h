// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_LOCATION_BAR_STUB_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_LOCATION_BAR_STUB_H_

#include <optional>
#include <string>

#include "chrome/browser/ui/location_bar/location_bar.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

class BrowserWindowInterface;
class ChipController;
class LocationBarModel;
class LocationBarTesting;
class OmniboxController;
class OmniboxPopupView;
class OmniboxView;
class Profile;

namespace bubble_anchor_util {
struct AnchorConfiguration;
}

namespace content {
class WebContents;
}

namespace ui {
class MouseEvent;
class TrackedElement;
}  // namespace ui

namespace contextual_tasks {

// A helper class that stubs out all unused LocationBar methods by default.
class LocationBarStub : public LocationBar {
 public:
  LocationBarStub();
  ~LocationBarStub() override;

  // LocationBar:
  void FocusLocation(bool is_user_initiated,
                     bool clear_focus_if_failed) override;
  void FocusSearch() override;
  void UpdateFocusBehavior(bool toolbar_visible) override;
  void UpdateContentSettingsIcons() override;
  void SaveStateToContents(content::WebContents* contents) override;
  void Revert() override;
  OmniboxView* GetOmniboxView() override;
  OmniboxPopupView* GetOmniboxPopupView() override;
  OmniboxController* GetOmniboxController() override;
  bool ShouldCloseOmniboxPopup(ui::MouseEvent* event) override;
  content::WebContents* GetWebContents() override;
  LocationBarModel* GetLocationBarModel() override;
  std::optional<bubble_anchor_util::AnchorConfiguration> GetChipAnchor()
      override;
  ChipController* GetChipController() override;
  void AnnounceAlert(const std::u16string& announcement) override;
  void OnChanged() override;
  void UpdateWithoutTabRestore() override;
  ui::TrackedElement* GetAnchorOrNull() override;
  BrowserWindowInterface* GetBrowser() override;
  Profile* GetProfile() override;
  bool IsInitialized() const override;
  bool IsVisible() const override;
  bool IsDrawn() const override;
  bool IsFullscreen() const override;
  bool IsEditingOrEmpty() const override;
  bool IsMouseHovered() const override;
  bool IsFocusWithin() const override;
  void InvalidateLayout() override;
  gfx::Rect Bounds() const override;
  gfx::Rect BoundsInScreen() const override;
  gfx::Size MinimumSize() const override;
  gfx::Size PreferredSize() const override;
  void Update(content::WebContents* contents) override;
  void ResetTabState(content::WebContents* contents) override;
  bool HasSecurityStateChanged() override;
  LocationBarTesting* GetLocationBarForTesting() override;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_LOCATION_BAR_STUB_H_
