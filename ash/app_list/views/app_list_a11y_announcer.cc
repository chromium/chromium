// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_a11y_announcer.h"

#include <memory>

#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"

namespace ash {

AppListA11yAnnouncer::AppListA11yAnnouncer(views::View* announcement_view)
    : announcement_view_(announcement_view) {}

AppListA11yAnnouncer::~AppListA11yAnnouncer() = default;

void AppListA11yAnnouncer::Shutdown() {
  announcement_view_ = nullptr;
}

void AppListA11yAnnouncer::AnnounceAppListShown() {
  Announce(l10n_util::GetStringUTF16(
      IDS_APP_LIST_ALL_APPS_ACCESSIBILITY_ANNOUNCEMENT));
}

void AppListA11yAnnouncer::AnnounceItemNotificationBadge(
    const std::u16string& selected_view_title) {
  Announce(l10n_util::GetStringFUTF16(IDS_APP_LIST_APP_FOCUS_NOTIFICATION_BADGE,
                                      selected_view_title));
}

void AppListA11yAnnouncer::AnnounceFolderDrop(
    const std::u16string& moving_view_title,
    const std::u16string& target_view_title,
    bool target_is_folder) {
  Announce(l10n_util::GetStringFUTF16(
      target_is_folder ? IDS_APP_LIST_APP_DRAG_MOVE_TO_FOLDER_ACCESSIBILE_NAME
                       : IDS_APP_LIST_APP_DRAG_CREATE_FOLDER_ACCESSIBILE_NAME,
      moving_view_title, target_view_title));
}

void AppListA11yAnnouncer::AnnounceKeyboardFoldering(
    const std::u16string& moving_view_title,
    const std::u16string& target_view_title,
    bool target_is_folder) {
  Announce(l10n_util::GetStringFUTF16(
      target_is_folder
          ? IDS_APP_LIST_APP_KEYBOARD_MOVE_TO_FOLDER_ACCESSIBILE_NAME
          : IDS_APP_LIST_APP_KEYBOARD_CREATE_FOLDER_ACCESSIBILE_NAME,
      moving_view_title, target_view_title));
}

void AppListA11yAnnouncer::AnnounceAppsGridReorder(int target_page,
                                                   int target_row,
                                                   int target_column) {
  Announce(l10n_util::GetStringFUTF16(
      IDS_APP_LIST_APP_DRAG_LOCATION_ACCESSIBILE_NAME,
      base::NumberToString16(target_page), base::NumberToString16(target_row),
      base::NumberToString16(target_column)));
}

void AppListA11yAnnouncer::AnnounceAppsGridReorder(int target_row,
                                                   int target_column) {
  Announce(l10n_util::GetStringFUTF16(
      IDS_APP_LIST_APP_DRAG_ROW_COLUMN_ACCESSIBILE_NAME,
      base::NumberToString16(target_row),
      base::NumberToString16(target_column)));
}

void AppListA11yAnnouncer::AnnounceFolderClosed() {
  Announce(l10n_util::GetStringUTF16(
      IDS_APP_LIST_FOLDER_CLOSE_FOLDER_ACCESSIBILE_NAME));
}

void AppListA11yAnnouncer::Announce(const std::u16string& announcement) {
  if (!announcement_view_)
    return;

  announcement_view_->GetViewAccessibility().AnnounceText(announcement);
}

}  // namespace ash
