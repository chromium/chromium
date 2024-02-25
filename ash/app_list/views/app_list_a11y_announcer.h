// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_A11Y_ANNOUNCER_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_A11Y_ANNOUNCER_H_

#include <string>

#include "base/memory/raw_ptr.h"

namespace views {
class View;
}

namespace ash {

// Wrapper for a view used to send accessibility alerts within the app list UI.
class AppListA11yAnnouncer {
 public:
  // `announcement_view` is the view that will be used to send accessibility
  // alerts. The `AppListA11yAnnouncer` owner is expected to ensure that
  // `annoucement_view` remains valid while the announcer can be used.
  explicit AppListA11yAnnouncer(views::View* announcement_view);
  AppListA11yAnnouncer(const AppListA11yAnnouncer&) = delete;
  AppListA11yAnnouncer& operator=(const AppListA11yAnnouncer&) = delete;
  ~AppListA11yAnnouncer();

  // Resets the announcer - all announcement methods become no-op. Used to clear
  // the reference to `announcement_view_` when the view is about to get
  // deleted.
  void Shutdown();

  // Modifies the announcement view to verbalize that app list is activated.
  void AnnounceAppListShown();

  // Modifies the announcement view to verbalize that the focused view has new
  // updates, based on the item having a notification badge.
  void AnnounceItemNotificationBadge(const std::u16string& selected_view_title);

  // Modifies the announcement view to verbalize that the current drag will move
  // |moving_view_title| and create a folder or move it into an existing folder
  // with |target_view_title|.
  void AnnounceFolderDrop(const std::u16string& moving_view_title,
                          const std::u16string& target_view_title,
                          bool target_is_folder);

  // Modifies the announcement view to verbalize that the most recent keyboard
  // foldering action has either moved |moving_view_title| into
  // |target_view_title| folder or that |moving_view_title| and
  // |target_view_title| have formed a new folder.
  void AnnounceKeyboardFoldering(const std::u16string& moving_view_title,
                                 const std::u16string& target_view_title,
                                 bool target_is_folder);

  // Modifies the announcement view to verbalize that an apps grid item has been
  // reordered to |target_row| and |target_column| within the |target_page| in
  // the apps grid.
  void AnnounceAppsGridReorder(int target_page,
                               int target_row,
                               int target_column);

  // As above, but does not announce a page. Used for single-page apps grids.
  void AnnounceAppsGridReorder(int target_row, int target_column);

  // Modifies the announcement view to verbalize that a folder was closed in the
  // apps container.
  void AnnounceFolderClosed();

  // Modifies the announcement view to verbalize the provided announcement.
  void Announce(const std::u16string& announcement);

 private:
  // The view used to send accessibility announcements. Owned by the parent's
  // views hierarchy.
  raw_ptr<views::View> announcement_view_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_A11Y_ANNOUNCER_H_
