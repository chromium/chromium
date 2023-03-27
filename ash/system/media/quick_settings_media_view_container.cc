// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/quick_settings_media_view_container.h"

#include "ash/system/tray/tray_constants.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {
constexpr int kContainerHeight = 150;
}  // namespace

QuickSettingsMediaViewContainer::QuickSettingsMediaViewContainer() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

void QuickSettingsMediaViewContainer::SetShowMediaView(bool show_media_view) {
  show_media_view_ = show_media_view;
  MaybeShowMediaView();
}

void QuickSettingsMediaViewContainer::MaybeShowMediaView() {
  SetVisible(show_media_view_);
}

int QuickSettingsMediaViewContainer::GetExpandedHeight() const {
  return show_media_view_ ? kContainerHeight : 0;
}

gfx::Size QuickSettingsMediaViewContainer::CalculatePreferredSize() const {
  return gfx::Size(kTrayMenuWidth, GetExpandedHeight());
}

}  // namespace ash