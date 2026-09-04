// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/location_bar_stub.h"

#include "chrome/browser/ui/views/bubble_anchor_util_views.h"

namespace contextual_tasks {

LocationBarStub::LocationBarStub() : LocationBar(nullptr) {}

LocationBarStub::~LocationBarStub() = default;

void LocationBarStub::FocusLocation(bool is_user_initiated,
                                    bool clear_focus_if_failed) {}

void LocationBarStub::FocusSearch() {}

void LocationBarStub::UpdateFocusBehavior(bool toolbar_visible) {}

void LocationBarStub::UpdateContentSettingsIcons() {}

void LocationBarStub::SaveStateToContents(content::WebContents* contents) {}

void LocationBarStub::Revert() {}

OmniboxView* LocationBarStub::GetOmniboxView() {
  return nullptr;
}

OmniboxPopupView* LocationBarStub::GetOmniboxPopupView() {
  return nullptr;
}

OmniboxController* LocationBarStub::GetOmniboxController() {
  return nullptr;
}

bool LocationBarStub::ShouldCloseOmniboxPopup(ui::MouseEvent* event) {
  return false;
}

content::WebContents* LocationBarStub::GetWebContents() {
  return nullptr;
}

LocationBarModel* LocationBarStub::GetLocationBarModel() {
  return nullptr;
}

std::optional<bubble_anchor_util::AnchorConfiguration>
LocationBarStub::GetChipAnchor() {
  return std::nullopt;
}

ChipController* LocationBarStub::GetChipController() {
  return nullptr;
}

void LocationBarStub::AnnounceAlert(const std::u16string& announcement) {}

void LocationBarStub::OnChanged() {}

void LocationBarStub::UpdateWithoutTabRestore() {}

ui::TrackedElement* LocationBarStub::GetAnchorOrNull() {
  return nullptr;
}

BrowserWindowInterface* LocationBarStub::GetBrowser() {
  return nullptr;
}

Profile* LocationBarStub::GetProfile() {
  return nullptr;
}

bool LocationBarStub::IsInitialized() const {
  return true;
}

bool LocationBarStub::IsVisible() const {
  return true;
}

bool LocationBarStub::IsDrawn() const {
  return true;
}

bool LocationBarStub::IsFullscreen() const {
  return false;
}

bool LocationBarStub::IsEditingOrEmpty() const {
  return false;
}

bool LocationBarStub::IsMouseHovered() const {
  return false;
}

bool LocationBarStub::IsFocusWithin() const {
  return false;
}

void LocationBarStub::InvalidateLayout() {}

gfx::Rect LocationBarStub::Bounds() const {
  return gfx::Rect();
}

gfx::Rect LocationBarStub::BoundsInScreen() const {
  return gfx::Rect();
}

gfx::Size LocationBarStub::MinimumSize() const {
  return gfx::Size();
}

gfx::Size LocationBarStub::PreferredSize() const {
  return gfx::Size();
}

void LocationBarStub::Update(content::WebContents* contents) {}

void LocationBarStub::ResetTabState(content::WebContents* contents) {}

bool LocationBarStub::HasSecurityStateChanged() {
  return false;
}

LocationBarTesting* LocationBarStub::GetLocationBarForTesting() {
  return nullptr;
}

}  // namespace contextual_tasks
