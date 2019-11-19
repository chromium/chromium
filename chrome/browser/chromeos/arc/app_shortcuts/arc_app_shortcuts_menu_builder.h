// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_APP_SHORTCUTS_ARC_APP_SHORTCUTS_MENU_BUILDER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_APP_SHORTCUTS_ARC_APP_SHORTCUTS_MENU_BUILDER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/arc/app_shortcuts/arc_app_shortcut_item.h"

class Profile;

namespace ui {
class SimpleMenuModel;
}  // namespace ui

namespace arc {

class ArcAppShortcutsRequest;

// A helper class that builds Android app shortcut items on context menu model.
class ArcAppShortcutsMenuBuilder {
 public:
  ArcAppShortcutsMenuBuilder(Profile* profile,
                             const std::string& app_id,
                             int64_t display_id,
                             int command_id_first,
                             int command_id_last);
  ~ArcAppShortcutsMenuBuilder();

  // Builds arc app shortcuts menu.
  using GetMenuModelCallback =
      base::OnceCallback<void(std::unique_ptr<ui::SimpleMenuModel>)>;
  void BuildMenu(const std::string& package_name,
                 std::unique_ptr<ui::SimpleMenuModel> menu_model,
                 GetMenuModelCallback callback);

  // Executes arc app shortcuts menu.
  void ExecuteCommand(int command_id);

 private:
  // Bound by |arc_app_shortcuts_request_|'s OnGetAppShortcutItems method.
  void OnGetAppShortcutItems(
      const base::TimeTicks& start_time,
      std::unique_ptr<ui::SimpleMenuModel> menu_model,
      GetMenuModelCallback callback,
      std::unique_ptr<ArcAppShortcutItems> app_shortcut_items);

  Profile* const profile_;
  const std::string app_id_;
  const int64_t display_id_;
  const int command_id_first_;
  const int command_id_last_;

  // Caches the app shortcut items from OnGetAppShortcutItems().
  std::unique_ptr<ArcAppShortcutItems> app_shortcut_items_;

  // Handles requesting app shortcuts from Android.
  std::unique_ptr<ArcAppShortcutsRequest> arc_app_shortcuts_request_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppShortcutsMenuBuilder);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_APP_SHORTCUTS_ARC_APP_SHORTCUTS_MENU_BUILDER_H_
