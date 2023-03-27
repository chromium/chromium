// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/quick_settings_media_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/media/quick_settings_media_view_controller.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

QuickSettingsMediaView::QuickSettingsMediaView(
    QuickSettingsMediaViewController* controller)
    : controller_(controller) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_SETTINGS_BUBBLE_MEDIA_CONTROLS_SUB_MENU_ACCESSIBLE_DESCRIPTION));
}

QuickSettingsMediaView::~QuickSettingsMediaView() = default;

void QuickSettingsMediaView::ShowItem(
    const std::string& id,
    std::unique_ptr<global_media_controls::MediaItemUIView> item) {
  DCHECK(!base::Contains(items_, id));
  items_[id] = AddChildView(std::move(item));
  PreferredSizeChanged();
  controller_->SetShowMediaView(true);
}

void QuickSettingsMediaView::HideItem(const std::string& id) {
  if (!base::Contains(items_, id)) {
    return;
  }
  RemoveChildView(items_[id]);
  delete items_[id];
  items_.erase(id);
  PreferredSizeChanged();
  controller_->SetShowMediaView(!items_.empty());
}

}  // namespace ash