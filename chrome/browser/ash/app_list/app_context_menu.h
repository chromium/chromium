// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_CONTEXT_MENU_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_CONTEXT_MENU_H_

#include <memory>
#include <string>

#include "ash/public/cpp/app_menu_constants.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_menu_model.h"

class AppListControllerDelegate;
class Profile;

namespace ash {
enum class AppListItemContext;
}

namespace app_list {

class AppContextMenuDelegate;

// Base class of all context menus in app list view.
class AppContextMenu : public ui::SimpleMenuModel::Delegate {
 public:
  AppContextMenu(AppContextMenuDelegate* delegate,
                 Profile* profile,
                 const std::string& app_id,
                 AppListControllerDelegate* controller,
                 ash::AppListItemContext item_context);
  AppContextMenu(const AppContextMenu&) = delete;
  AppContextMenu& operator=(const AppContextMenu&) = delete;
  ~AppContextMenu() override;

  using GetMenuModelCallback =
      base::OnceCallback<void(std::unique_ptr<ui::SimpleMenuModel>)>;
  virtual void GetMenuModel(GetMenuModelCallback callback);

  // ui::SimpleMenuModel::Delegate overrides:
  bool IsItemForCommandIdDynamic(int command_id) const override;
  std::u16string GetLabelForCommandId(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  ui::ImageModel GetIconForCommandId(int command_id) const override;

  // Helper method to get the gfx::VectorIcon for a |command_id|. Returns an
  // empty gfx::VectorIcon if there is no icon for this |command_id|.
  static const gfx::VectorIcon& GetMenuItemVectorIcon(int command_id,
                                                      int string_id);

 protected:
  // Creates default items, derived class may override to add their specific
  // items.
  virtual void BuildMenu(ui::SimpleMenuModel* menu_model);

  // Helper that toggles pinning state of provided app.
  void TogglePin(const std::string& shelf_app_id);

  // Helper method to add touchable or normal context menu options.
  void AddContextMenuOption(ui::SimpleMenuModel* menu_model,
                            ash::CommandId command_id,
                            int string_id);
  // Helper method to add reorder context menu options.
  void AddReorderMenuOption(ui::SimpleMenuModel* menu_model);

  const std::string& app_id() const { return app_id_; }
  Profile* profile() const { return profile_; }
  AppContextMenuDelegate* delegate() const { return delegate_; }
  AppListControllerDelegate* controller() const { return controller_; }

 private:
  raw_ptr<AppContextMenuDelegate> delegate_;
  raw_ptr<Profile> profile_;
  const std::string app_id_;
  raw_ptr<AppListControllerDelegate, DanglingUntriaged> controller_;

  // The SimpleMenuModel that contains reorder options. Could be nullptr if
  // sorting is not available.
  std::unique_ptr<ui::SimpleMenuModel> reorder_submenu_;

  // Where this item is being shown (e.g. the apps grid or recent apps).
  const ash::AppListItemContext item_context_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_CONTEXT_MENU_H_
