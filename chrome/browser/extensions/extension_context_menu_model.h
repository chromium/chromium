// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTEXT_MENU_MODEL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTEXT_MENU_MODEL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "extensions/common/extension_id.h"
#include "ui/base/models/simple_menu_model.h"
#include "url/origin.h"

class Browser;
class Profile;

namespace content {
class WebContents;
}

namespace extensions {
class ContextMenuMatcher;
class Extension;
class ExtensionAction;
class SidePanelService;

// The context menu model for extension icons.
class ExtensionContextMenuModel : public ui::SimpleMenuModel,
                                  public ui::SimpleMenuModel::Delegate {
 public:
  enum MenuEntries {
    HOME_PAGE = 0,
    OPTIONS,
    TOGGLE_VISIBILITY,
    UNINSTALL,
    MANAGE_EXTENSIONS,
    INSPECT_POPUP,
    PAGE_ACCESS_CANT_ACCESS,
    PAGE_ACCESS_SUBMENU,
    PAGE_ACCESS_RUN_ON_CLICK,
    PAGE_ACCESS_RUN_ON_SITE,
    PAGE_ACCESS_RUN_ON_ALL_SITES,
    PAGE_ACCESS_LEARN_MORE,
    PAGE_ACCESS_ALL_EXTENSIONS_GRANTED,
    PAGE_ACCESS_ALL_EXTENSIONS_BLOCKED,
    PAGE_ACCESS_PERMISSIONS_PAGE,
    VIEW_WEB_PERMISSIONS,
    POLICY_INSTALLED,
    TOGGLE_SIDE_PANEL_VISIBILITY,
    // NOTE: If you update this, you probably need to update the
    // ContextMenuAction enum below.
  };

  // A separate enum to indicate the action taken on the menu. We have two
  // enums (this and MenuEntries above) to avoid needing to have a single one
  // with both action-specific values (like kNoAction) and menu-specific values
  // (like PAGE_ACCESS_SUBMENU).
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. New values should be added before
  // kMaxValue.
  enum class ContextMenuAction {
    kNoAction = 0,
    kCustomCommand = 1,
    kHomePage = 2,
    kOptions = 3,
    kToggleVisibility = 4,
    kUninstall = 5,
    kManageExtensions = 6,
    kInspectPopup = 7,
    kPageAccessRunOnClick = 8,
    kPageAccessRunOnSite = 9,
    kPageAccessRunOnAllSites = 10,
    kPageAccessLearnMore = 11,
    kPageAccessPermissionsPage = 12,
    kViewWebPermissions = 13,
    kPolicyInstalled = 14,
    kToggleSidePanelVisibility = 15,
    kMaxValue = kToggleSidePanelVisibility,
    // NOTE: Please update ExtensionContextMenuAction in enums.xml if you modify
    // this enum.
  };

  // Location where the context menu is open from.
  enum class ContextMenuSource { kToolbarAction = 0, kMenuItem = 1 };

  // Delegate to handle showing an ExtensionAction popup.
  class PopupDelegate {
   public:
    // Called when the user selects the menu item which requests that the
    // popup be shown and inspected.
    // The delegate should know which popup to display.
    virtual void InspectPopup() = 0;

   protected:
    virtual ~PopupDelegate() = default;
  };

  // Creates a menu model for the given extension. If
  // prefs::kExtensionsUIDeveloperMode is enabled then a menu item
  // will be shown for "Inspect Popup" which, when selected, will cause
  // ShowPopupForDevToolsWindow() to be called on |delegate|.
  ExtensionContextMenuModel(const Extension* extension,
                            Browser* browser,
                            bool is_pinned,
                            PopupDelegate* delegate,
                            bool can_show_icon_in_toolbar,
                            ContextMenuSource source);

  ExtensionContextMenuModel(const ExtensionContextMenuModel&) = delete;
  ExtensionContextMenuModel& operator=(const ExtensionContextMenuModel&) =
      delete;

  ~ExtensionContextMenuModel() override;

  // SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void OnMenuWillShow(ui::SimpleMenuModel* source) override;
  void MenuClosed(ui::SimpleMenuModel* source) override;

  ui::SimpleMenuModel* page_access_submenu_for_testing() {
    return page_access_submenu_.get();
  }

 private:
  void InitMenu(const Extension* extension, bool can_show_icon_in_toolbar);

  // Constructs the menu when `kExtensionsMenuAccessControl` is enabled.
  void InitMenuWithFeature(const Extension* extension,
                           bool can_show_icon_in_toolbar);

  // Adds the page access items based on the current site setting pointed by
  // `web_contents`.
  void CreatePageAccessItems(const Extension* extension,
                             content::WebContents* web_contents);

  // Gets the extension we are displaying the menu for. Returns NULL if the
  // extension has been uninstalled and no longer exists.
  const Extension* GetExtension() const;

  // Returns the active web contents.
  content::WebContents* GetActiveWebContents() const;

  // Returns the side panel service for the current profile.
  SidePanelService* GetSidePanelService() const;

  // Appends the extension's context menu items.
  void AppendExtensionItems();

  // Appends the side panel menu item to the context menu if `extension` has one
  // it can open.
  void AddSidePanelEntryIfPresent(const Extension& extension);

  // A copy of the extension's id.
  ExtensionId extension_id_;

  // Whether the menu is for a component extension.
  bool is_component_;

  // The extension action of the extension we are displaying the menu for (if
  // it has one, otherwise NULL).
  raw_ptr<ExtensionAction, DanglingUntriaged> extension_action_;

  const raw_ptr<Browser> browser_;

  raw_ptr<Profile> profile_;

  // The delegate which handles the 'inspect popup' menu command (or NULL).
  raw_ptr<PopupDelegate> delegate_;

  // Whether the extension icon is pinned at the time the menu opened.
  bool is_pinned_;

  // Menu matcher for context menu items specified by the extension.
  std::unique_ptr<ContextMenuMatcher> extension_items_;

  std::unique_ptr<ui::SimpleMenuModel> page_access_submenu_;

  // The action taken by the menu. Has a valid value when the menu is being
  // shown.
  std::optional<ContextMenuAction> action_taken_;

  ContextMenuSource source_;

  // The origin used to populate the context menu's content.
  // TODO(crbug.com/40265043): Web contents may change while the menu is open,
  // which may affect the context menu contents. We should dynamically update
  // the context menu, or close it when this happens.
  url::Origin origin_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTEXT_MENU_MODEL_H_
