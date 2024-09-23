// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_context_menu_model.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/side_panel/side_panel_service.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/extensions/permissions_url_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/extension_side_panel_utils.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/common/extensions/api/side_panel.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/url_formatter/url_formatter.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_separator_types.h"

namespace extensions {

namespace {

// Returns true if the given |item| is of the given |type|.
bool MenuItemMatchesAction(const std::optional<ActionInfo::Type> action_type,
                           const MenuItem* item) {
  if (!action_type)
    return false;

  const MenuItem::ContextList& contexts = item->contexts();

  if (contexts.Contains(MenuItem::ALL))
    return true;
  if (contexts.Contains(MenuItem::PAGE_ACTION) &&
      (*action_type == ActionInfo::Type::kPage)) {
    return true;
  }
  if (contexts.Contains(MenuItem::BROWSER_ACTION) &&
      (*action_type == ActionInfo::Type::kBrowser)) {
    return true;
  }
  if (contexts.Contains(MenuItem::ACTION) &&
      (*action_type == ActionInfo::Type::kAction)) {
    return true;
  }

  return false;
}

// Returns true if the given |extension| is required to remain pinned/visible in
// the toolbar by policy.
bool IsExtensionForcePinned(const Extension& extension, Profile* profile) {
  auto* management = ExtensionManagementFactory::GetForBrowserContext(profile);
  return base::Contains(management->GetForcePinnedList(), extension.id());
}

// Returns the id for the visibility command for the given |extension|.
int GetVisibilityStringId(Profile* profile,
                          const Extension* extension,
                          bool is_pinned) {
  if (IsExtensionForcePinned(*extension, profile)) {
    return IDS_EXTENSIONS_PINNED_BY_ADMIN;
  }
  return is_pinned ? IDS_EXTENSIONS_UNPIN_FROM_TOOLBAR
                   : IDS_EXTENSIONS_PIN_TO_TOOLBAR;
}

// Returns true if the given |extension| is required to remain installed by
// policy.
bool IsExtensionRequiredByPolicy(const Extension* extension, Profile* profile) {
  ManagementPolicy* policy = ExtensionSystem::Get(profile)->management_policy();
  return !policy->UserMayModifySettings(extension, nullptr) ||
         policy->MustRemainInstalled(extension, nullptr);
}

std::u16string GetCurrentSite(const GURL& url) {
  return url_formatter::IDNToUnicode(url_formatter::StripWWW(url.host()));
}

ExtensionContextMenuModel::ContextMenuAction CommandIdToContextMenuAction(
    int command_id) {
  using ContextMenuAction = ExtensionContextMenuModel::ContextMenuAction;

  switch (command_id) {
    case ExtensionContextMenuModel::HOME_PAGE:
      return ContextMenuAction::kHomePage;
    case ExtensionContextMenuModel::OPTIONS:
      return ContextMenuAction::kOptions;
    case ExtensionContextMenuModel::TOGGLE_VISIBILITY:
      return ContextMenuAction::kToggleVisibility;
    case ExtensionContextMenuModel::UNINSTALL:
      return ContextMenuAction::kUninstall;
    case ExtensionContextMenuModel::TOGGLE_SIDE_PANEL_VISIBILITY:
      return ContextMenuAction::kToggleSidePanelVisibility;
    case ExtensionContextMenuModel::MANAGE_EXTENSIONS:
      return ContextMenuAction::kManageExtensions;
    case ExtensionContextMenuModel::INSPECT_POPUP:
      return ContextMenuAction::kInspectPopup;
    case ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_CLICK:
      return ContextMenuAction::kPageAccessRunOnClick;
    case ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_SITE:
      return ContextMenuAction::kPageAccessRunOnSite;
    case ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_ALL_SITES:
      return ContextMenuAction::kPageAccessRunOnAllSites;
    case ExtensionContextMenuModel::PAGE_ACCESS_PERMISSIONS_PAGE:
      return ContextMenuAction::kPageAccessPermissionsPage;
    case ExtensionContextMenuModel::PAGE_ACCESS_LEARN_MORE:
      return ContextMenuAction::kPageAccessLearnMore;
    case ExtensionContextMenuModel::PAGE_ACCESS_CANT_ACCESS:
    case ExtensionContextMenuModel::PAGE_ACCESS_SUBMENU:
    case ExtensionContextMenuModel::PAGE_ACCESS_ALL_EXTENSIONS_GRANTED:
    case ExtensionContextMenuModel::PAGE_ACCESS_ALL_EXTENSIONS_BLOCKED:
      DUMP_WILL_BE_NOTREACHED();
      break;
    case ExtensionContextMenuModel::VIEW_WEB_PERMISSIONS:
      return ContextMenuAction::kViewWebPermissions;
    case ExtensionContextMenuModel::POLICY_INSTALLED:
      return ContextMenuAction::kPolicyInstalled;
    default:
      break;
  }
  DUMP_WILL_BE_NOTREACHED();
  return ContextMenuAction::kNoAction;
}

PermissionsManager::UserSiteAccess CommandIdToSiteAccess(int command_id) {
  switch (command_id) {
    case ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_CLICK:
      return PermissionsManager::UserSiteAccess::kOnClick;
    case ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_SITE:
      return PermissionsManager::UserSiteAccess::kOnSite;
    case ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_ALL_SITES:
      return PermissionsManager::UserSiteAccess::kOnAllSites;
  }
  NOTREACHED_IN_MIGRATION();
  return PermissionsManager::UserSiteAccess::kOnClick;
}

// Logs a user action when an option is selected in the page access section of
// the context menu.
void LogPageAccessAction(int command_id) {
  switch (command_id) {
    case ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_CLICK:
      base::RecordAction(base::UserMetricsAction(
          "Extensions.ContextMenu.Hosts.OnClickClicked"));
      break;
    case ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_SITE:
      base::RecordAction(base::UserMetricsAction(
          "Extensions.ContextMenu.Hosts.OnSiteClicked"));
      break;
    case ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_ALL_SITES:
      base::RecordAction(base::UserMetricsAction(
          "Extensions.ContextMenu.Hosts.OnAllSitesClicked"));
      break;
    case ExtensionContextMenuModel::PAGE_ACCESS_PERMISSIONS_PAGE:
      base::RecordAction(base::UserMetricsAction(
          "Extensions.ContextMenu.Hosts.PermissionsPageClicked"));
      break;
    case ExtensionContextMenuModel::PAGE_ACCESS_LEARN_MORE:
      base::RecordAction(base::UserMetricsAction(
          "Extensions.ContextMenu.Hosts.LearnMoreClicked"));
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Unknown option: " << command_id;
      break;
  }
}

// Logs the action's visibility in the toolbar after it was set to `visible`.
void LogToggleVisibility(bool visible) {
  if (visible) {
    base::RecordAction(
        base::UserMetricsAction("Extensions.ContextMenu.PinExtension"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("Extensions.ContextMenu.UnpinExtension"));
  }
}

void OpenUrl(Browser& browser, const GURL& url) {
  content::OpenURLParams params(
      url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false);
  browser.OpenURL(params, /*navigation_handle_callback=*/{});
}

// A stub for the uninstall dialog.
// TODO(devlin): Ideally, we would just have the uninstall dialog take a
// base::OnceCallback, but that's a bunch of churn.
class UninstallDialogHelper : public ExtensionUninstallDialog::Delegate {
 public:
  UninstallDialogHelper(const UninstallDialogHelper&) = delete;
  UninstallDialogHelper& operator=(const UninstallDialogHelper&) = delete;

  // Kicks off the asynchronous process to confirm and uninstall the given
  // |extension|.
  static void UninstallExtension(Browser* browser, const Extension* extension) {
    UninstallDialogHelper* helper = new UninstallDialogHelper();
    helper->BeginUninstall(browser, extension);
  }

 private:
  // This class handles its own lifetime.
  UninstallDialogHelper() = default;
  ~UninstallDialogHelper() override = default;

  void BeginUninstall(Browser* browser, const Extension* extension) {
    uninstall_dialog_ = ExtensionUninstallDialog::Create(
        browser->profile(), browser->window()->GetNativeWindow(), this);
    uninstall_dialog_->ConfirmUninstall(extension,
                                        UNINSTALL_REASON_USER_INITIATED,
                                        UNINSTALL_SOURCE_TOOLBAR_CONTEXT_MENU);
  }

  // ExtensionUninstallDialog::Delegate:
  void OnExtensionUninstallDialogClosed(bool did_start_uninstall,
                                        const std::u16string& error) override {
    delete this;
  }

  std::unique_ptr<ExtensionUninstallDialog> uninstall_dialog_;
};

}  // namespace

ExtensionContextMenuModel::ExtensionContextMenuModel(
    const Extension* extension,
    Browser* browser,
    bool is_pinned,
    PopupDelegate* delegate,
    bool can_show_icon_in_toolbar,
    ContextMenuSource source)
    : SimpleMenuModel(this),
      extension_id_(extension->id()),
      is_component_(Manifest::IsComponentLocation(extension->location())),
      browser_(browser),
      profile_(browser->profile()),
      delegate_(delegate),
      is_pinned_(is_pinned),
      source_(source) {
  if (GetActiveWebContents()) {
    origin_ =
        url::Origin::Create(GetActiveWebContents()->GetLastCommittedURL());
  }

  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    InitMenuWithFeature(extension, can_show_icon_in_toolbar);
  } else {
    InitMenu(extension, can_show_icon_in_toolbar);
  }
}

bool ExtensionContextMenuModel::IsCommandIdChecked(int command_id) const {
  const Extension* extension = GetExtension();
  if (!extension)
    return false;

  if (ContextMenuMatcher::IsExtensionsCustomCommandId(command_id))
    return extension_items_->IsCommandIdChecked(command_id);

  if (command_id == PAGE_ACCESS_RUN_ON_CLICK ||
      command_id == PAGE_ACCESS_RUN_ON_SITE ||
      command_id == PAGE_ACCESS_RUN_ON_ALL_SITES) {
    auto* permissions = PermissionsManager::Get(profile_);
    PermissionsManager::UserSiteAccess current_access =
        permissions->GetUserSiteAccess(*extension, origin_.GetURL());
    return current_access == CommandIdToSiteAccess(command_id);
  }

  return false;
}

bool ExtensionContextMenuModel::IsCommandIdVisible(int command_id) const {
  const Extension* extension = GetExtension();
  if (!extension)
    return false;

  if (ContextMenuMatcher::IsExtensionsCustomCommandId(command_id))
    return extension_items_->IsCommandIdVisible(command_id);

  // Items added by Chrome to the menu are always visible.
  return true;
}

bool ExtensionContextMenuModel::IsCommandIdEnabled(int command_id) const {
  const Extension* extension = GetExtension();
  if (!extension)
    return false;

  if (ContextMenuMatcher::IsExtensionsCustomCommandId(command_id))
    return extension_items_->IsCommandIdEnabled(command_id);

  switch (command_id) {
    case HOME_PAGE:
      // The HOME_PAGE links to the Homepage URL. If the extension doesn't have
      // a homepage, we just disable this menu item. We also disable for
      // component extensions, because it doesn't make sense to link to a
      // webstore page or chrome://extensions.
      return ManifestURL::GetHomepageURL(extension).is_valid() &&
             !is_component_;
    case OPTIONS:
      // Options is always enabled since it will only be visible if it has an
      // options page.
      DCHECK(OptionsPageInfo::HasOptionsPage(extension));
      return true;
    case INSPECT_POPUP: {
      content::WebContents* web_contents = GetActiveWebContents();
      return web_contents && extension_action_ &&
             extension_action_->HasPopup(
                 sessions::SessionTabHelper::IdForTab(web_contents).id());
    }
    case UNINSTALL:
      // Uninstall is always enabled since it will only be visible when the
      // extension can be removed.
      return true;
    case TOGGLE_SIDE_PANEL_VISIBILITY:
      // This option is always enabled since it will only be visible when the
      // extension provides a side panel.
      return true;
    case POLICY_INSTALLED:
      // This option is always disabled since user cannot remove a policy
      // installed extension.
      return false;
    case PAGE_ACCESS_CANT_ACCESS:
    case PAGE_ACCESS_ALL_EXTENSIONS_GRANTED:
    case PAGE_ACCESS_ALL_EXTENSIONS_BLOCKED:
      // When these commands are shown, they are always disabled.
      return false;
    case PAGE_ACCESS_SUBMENU:
    case PAGE_ACCESS_PERMISSIONS_PAGE:
    case PAGE_ACCESS_LEARN_MORE:
      // When these commands are shown, they are always enabled.
      return true;
    case PAGE_ACCESS_RUN_ON_CLICK:
    case PAGE_ACCESS_RUN_ON_SITE:
    case PAGE_ACCESS_RUN_ON_ALL_SITES:
      return PermissionsManager::Get(profile_)->CanUserSelectSiteAccess(
          *extension, origin_.GetURL(), CommandIdToSiteAccess(command_id));
    // Extension pinning/unpinning is not available for Incognito as this
    // leaves a trace of user activity.
    case TOGGLE_VISIBILITY:
      return !browser_->profile()->IsOffTheRecord() &&
             !IsExtensionForcePinned(*extension, profile_);
    // Manage extensions and view web permissions are always enabled.
    case MANAGE_EXTENSIONS:
    case VIEW_WEB_PERMISSIONS:
      return true;
    default:
      NOTREACHED_IN_MIGRATION() << "Unknown command" << command_id;
  }
  return true;
}

void ExtensionContextMenuModel::ExecuteCommand(int command_id,
                                               int event_flags) {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  if (ContextMenuMatcher::IsExtensionsCustomCommandId(command_id)) {
    DCHECK(extension_items_);
    extension_items_->ExecuteCommand(command_id, GetActiveWebContents(),
                                     nullptr, content::ContextMenuParams());
    action_taken_ = ContextMenuAction::kCustomCommand;
    return;
  }

  action_taken_ = CommandIdToContextMenuAction(command_id);

  switch (command_id) {
    case HOME_PAGE: {
      OpenUrl(*browser_, ManifestURL::GetHomepageURL(extension));
      break;
    }
    case OPTIONS:
      DCHECK(OptionsPageInfo::HasOptionsPage(extension));
      ExtensionTabUtil::OpenOptionsPage(extension, browser_);
      break;
    case TOGGLE_VISIBILITY: {
      bool visible = !is_pinned_;
      ToolbarActionsModel::Get(browser_->profile())
          ->SetActionVisibility(extension->id(), visible);
      LogToggleVisibility(visible);
      break;
    }
    case UNINSTALL: {
      UninstallDialogHelper::UninstallExtension(browser_, extension);
      break;
    }
    case TOGGLE_SIDE_PANEL_VISIBILITY: {
      // Do nothing if the web contents have navigated to a different origin.
      auto* web_contents = GetActiveWebContents();
      if (!web_contents ||
          !origin_.IsSameOriginWith(web_contents->GetLastCommittedURL())) {
        return;
      }

      SidePanelService* const side_panel_service = GetSidePanelService();
      CHECK(side_panel_service);

      // The state of the tab could have changed since we opened the context
      // menu. This check ensures that the extension has a valid side panel it
      // can open for `tab_id`.
      int tab_id = ExtensionTabUtil::GetTabId(GetActiveWebContents());
      if (side_panel_service->HasSidePanelContextMenuActionForTab(*extension,
                                                                  tab_id)) {
        side_panel_util::ToggleExtensionSidePanel(browser_, extension->id());
      }
      break;
    }
    case MANAGE_EXTENSIONS: {
      chrome::ShowExtensions(browser_, extension->id());
      break;
    }
    case VIEW_WEB_PERMISSIONS:
      chrome::ShowSiteSettings(browser_, extension->url());
      break;
    case INSPECT_POPUP: {
      delegate_->InspectPopup();
      break;
    }
    case POLICY_INSTALLED:
      // When visible, this option is always disabled.
      break;
    case PAGE_ACCESS_RUN_ON_CLICK:
    case PAGE_ACCESS_RUN_ON_SITE:
    case PAGE_ACCESS_RUN_ON_ALL_SITES: {
      // Do nothing if the web contents have navigated to a different origin.
      auto* web_contents = GetActiveWebContents();
      if (!web_contents ||
          !origin_.IsSameOriginWith(web_contents->GetLastCommittedURL())) {
        return;
      }

      LogPageAccessAction(command_id);

      // Do nothing if the extension cannot have its site permissions updated.
      // Page access option should only be enabled when the extension site
      // permissions can be changed. However, sometimes the command still gets
      // invoked (crbug.com/1468151). Thus, we exit early to prevent any
      // crashes.
      if (!PermissionsManager::Get(profile_)->CanAffectExtension(*extension)) {
        return;
      }

      SitePermissionsHelper permissions(profile_);
      permissions.UpdateSiteAccess(*extension, web_contents,
                                   CommandIdToSiteAccess(command_id));
      break;
    }
    case PAGE_ACCESS_PERMISSIONS_PAGE:
      LogPageAccessAction(command_id);
      OpenUrl(
          *browser_,
          GURL(extension_permissions_constants::kExtensionsSitePermissionsURL));
      break;
    case PAGE_ACCESS_LEARN_MORE:
      LogPageAccessAction(command_id);
      OpenUrl(
          *browser_,
          GURL(
              extension_permissions_constants::kRuntimeHostPermissionsHelpURL));

      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Unknown option";
      break;
  }
}

void ExtensionContextMenuModel::OnMenuWillShow(ui::SimpleMenuModel* menu) {
  action_taken_ = ContextMenuAction::kNoAction;
}

void ExtensionContextMenuModel::MenuClosed(ui::SimpleMenuModel* menu) {
  // `action_taken_` can be deleted when the extensions toggle menu is closed.
  if (action_taken_) {
    ContextMenuAction action = *action_taken_;
    bool was_side_panel_action_taken =
        action_taken_ == ContextMenuAction::kToggleSidePanelVisibility;
    UMA_HISTOGRAM_ENUMERATION("Extensions.ContextMenuAction", action);

    // Clear out the action to avoid any possible UAF if we close the parent
    // menu.
    action_taken_ = std::nullopt;
    if (source_ == ContextMenuSource::kMenuItem &&
        was_side_panel_action_taken) {
      browser_->window()->GetExtensionsContainer()->CloseOverflowMenuIfOpen();
      // WARNING: The overflow menu was the parent for this menu, so it's
      // possible `this` is now deleted.
    }
  }
}

ExtensionContextMenuModel::~ExtensionContextMenuModel() = default;

void ExtensionContextMenuModel::InitMenuWithFeature(
    const Extension* extension,
    bool can_show_icon_in_toolbar) {
  DCHECK(base::FeatureList::IsEnabled(
      extensions_features::kExtensionsMenuAccessControl));
  DCHECK(extension);

  extension_action_ =
      ExtensionActionManager::Get(profile_)->GetExtensionAction(*extension);
  std::optional<ActionInfo::Type> action_type =
      extension_action_
          ? std::optional<ActionInfo::Type>(extension_action_->action_type())
          : std::nullopt;

  extension_items_ = std::make_unique<ContextMenuMatcher>(
      profile_, this, this,
      base::BindRepeating(MenuItemMatchesAction, action_type));

  // Home page section.
  std::string extension_name = extension->name();
  // Ampersands need to be escaped to avoid being treated like
  // mnemonics in the menu.
  base::ReplaceChars(extension_name, "&", "&&", &extension_name);
  AddItem(HOME_PAGE, base::UTF8ToUTF16(extension_name));
  AppendExtensionItems();

  // Site permissions section.
  bool is_required_by_policy = IsExtensionRequiredByPolicy(extension, profile_);
  bool has_policy_entry = !is_component_ && is_required_by_policy;
  bool policy_entry_in_subpage = false;

  // Show section only when the extension requests host permissions or has
  // activeTab permission.
  auto* permissions_manager = PermissionsManager::Get(profile_);
  if (permissions_manager->HasRequestedHostPermissions(*extension) ||
      permissions_manager->HasRequestedActiveTab(*extension)) {
    content::WebContents* web_contents = GetActiveWebContents();
    const GURL& url = web_contents->GetLastCommittedURL();
    auto site_setting = permissions_manager->GetUserSiteSetting(origin_);

    if (site_setting ==
        PermissionsManager::UserSiteSetting::kGrantAllExtensions) {
      AddItem(
          PAGE_ACCESS_ALL_EXTENSIONS_GRANTED,
          l10n_util::GetStringFUTF16(
              IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_ALL_EXTENSIONS_GRANTED,
              GetCurrentSite(url)));
    } else if (site_setting ==
                   PermissionsManager::UserSiteSetting::kBlockAllExtensions &&
               !is_required_by_policy) {
      // An extension required by policy can have access when the user
      // blocked all extensions. Thus, we only show the 'all extensions blocked'
      // item for extensions not required by policy.
      AddItem(
          PAGE_ACCESS_ALL_EXTENSIONS_BLOCKED,
          l10n_util::GetStringFUTF16(
              IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_ALL_EXTENSIONS_BLOCKED,
              GetCurrentSite(url)));
    } else if (SitePermissionsHelper(profile_).GetSiteInteraction(
                   *extension, web_contents) ==
               SitePermissionsHelper::SiteInteraction::kNone) {
      // Extensions that don't request site access to this site have no site
      // interaction. Note: it's important this comes after handling the 'block
      // all extensions' site settings, since such setting changes all the
      // extensions site interaction to 'none' even if the extension requested
      // access to this site.
      AddItemWithStringId(PAGE_ACCESS_CANT_ACCESS,
                          IDS_EXTENSIONS_CONTEXT_MENU_CANT_ACCESS_PAGE);
    } else {
      // The extension wants site access and can run on the page. Add the three
      // site access options, which may be disabled.
      static constexpr int kRadioGroup = 0;
      page_access_submenu_ = std::make_unique<ui::SimpleMenuModel>(this);
      page_access_submenu_->AddRadioItemWithStringId(
          PAGE_ACCESS_RUN_ON_CLICK,
          IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_RUN_ON_CLICK_V2, kRadioGroup);
      page_access_submenu_->AddRadioItem(
          PAGE_ACCESS_RUN_ON_SITE,
          l10n_util::GetStringFUTF16(
              IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_RUN_ON_SITE_V2,
              GetCurrentSite(url)),
          kRadioGroup);
      page_access_submenu_->AddRadioItemWithStringId(
          PAGE_ACCESS_RUN_ON_ALL_SITES,
          IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_RUN_ON_ALL_SITES_V2,
          kRadioGroup);

      // We show the page access menu for force-installed extensions that
      // modify sites other than those the user opted into all extensions
      // modifying. In these cases, we indicate that the extension is installed
      // by the admin through a menu entry.
      if (has_policy_entry) {
        page_access_submenu_->AddSeparator(ui::NORMAL_SEPARATOR);
        page_access_submenu_->AddItemWithStringIdAndIcon(
            POLICY_INSTALLED, IDS_EXTENSIONS_INSTALLED_BY_ADMIN,
            ui::ImageModel::FromVectorIcon(vector_icons::kBusinessIcon,
                                           ui::kColorIcon, 16));
        policy_entry_in_subpage = true;
      }

      AddSubMenuWithStringId(PAGE_ACCESS_SUBMENU,
                             IDS_EXTENSIONS_CONTEXT_MENU_SITE_PERMISSIONS,
                             page_access_submenu_.get());
    }

    // Permissions page is always visible when the extension requests host
    // permissions.
    AddItemWithStringId(
        PAGE_ACCESS_PERMISSIONS_PAGE,
        IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_PERMISSIONS_PAGE);
  }

  // If there isn't an entry for the extension being force-installed in the
  // page access menu above, we add one to the root menu here.
  if (has_policy_entry && !policy_entry_in_subpage) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    // TODO (kylixrd): Investigate the usage of the hard-coded color.
    AddItemWithStringIdAndIcon(
        POLICY_INSTALLED, IDS_EXTENSIONS_INSTALLED_BY_ADMIN,
        ui::ImageModel::FromVectorIcon(vector_icons::kBusinessIcon,
                                       ui::kColorIcon, 16));
  }

  // Controls section.
  bool has_options_page = OptionsPageInfo::HasOptionsPage(extension);
  bool can_uninstall_extension = !is_component_ && !is_required_by_policy;
  if (can_show_icon_in_toolbar || has_options_page || can_uninstall_extension) {
    AddSeparator(ui::NORMAL_SEPARATOR);
  }

  if (can_show_icon_in_toolbar) {
    if (IsExtensionForcePinned(*extension, profile_)) {
      AddItemWithStringIdAndIcon(
          TOGGLE_VISIBILITY, IDS_EXTENSIONS_PINNED_BY_ADMIN,
          ui::ImageModel::FromVectorIcon(vector_icons::kBusinessIcon,
                                         ui::kColorIcon, 16));
    } else {
      int message_id = is_pinned_
                           ? IDS_EXTENSIONS_CONTEXT_MENU_UNPIN_FROM_TOOLBAR
                           : IDS_EXTENSIONS_CONTEXT_MENU_PIN_TO_TOOLBAR;
      AddItemWithStringId(TOGGLE_VISIBILITY, message_id);
    }
  }

  if (has_options_page) {
    AddItemWithStringId(OPTIONS, IDS_EXTENSIONS_OPTIONS_MENU_ITEM);
  }

  if (can_uninstall_extension) {
    AddItemWithStringId(UNINSTALL, IDS_EXTENSIONS_UNINSTALL);
  }

  AddSidePanelEntryIfPresent(*extension);

  // Settings section.
  if (!is_component_) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItemWithStringId(MANAGE_EXTENSIONS, IDS_MANAGE_EXTENSION);
    AddItemWithStringId(VIEW_WEB_PERMISSIONS, IDS_VIEW_WEB_PERMISSIONS);
  }

  // Developer section.
  const ActionInfo* action_info = ActionInfo::GetExtensionActionInfo(extension);
  if (delegate_ && !is_component_ && action_info && !action_info->synthesized &&
      profile_->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode)) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItemWithStringId(INSPECT_POPUP, IDS_EXTENSION_ACTION_INSPECT_POPUP);
  }
}

void ExtensionContextMenuModel::InitMenu(const Extension* extension,
                                         bool can_show_icon_in_toolbar) {
  DCHECK(extension);

  std::optional<ActionInfo::Type> action_type;
  extension_action_ =
      ExtensionActionManager::Get(profile_)->GetExtensionAction(*extension);
  if (extension_action_)
    action_type = extension_action_->action_type();

  extension_items_ = std::make_unique<ContextMenuMatcher>(
      profile_, this, this,
      base::BindRepeating(MenuItemMatchesAction, action_type));

  std::string extension_name = extension->name();
  // Ampersands need to be escaped to avoid being treated like
  // mnemonics in the menu.
  base::ReplaceChars(extension_name, "&", "&&", &extension_name);
  AddItem(HOME_PAGE, base::UTF8ToUTF16(extension_name));
  AppendExtensionItems();
  AddSeparator(ui::NORMAL_SEPARATOR);

  // Add page access items if active web contents exist and the extension
  // wants site access (either by requesting host permissions or active tab).
  auto* web_contents = GetActiveWebContents();
  auto* permissions_manager = PermissionsManager::Get(profile_);
  if (web_contents && (permissions_manager->CanAffectExtension(*extension) ||
                       permissions_manager->HasActiveTabAndCanAccess(
                           *extension, web_contents->GetLastCommittedURL()))) {
    CreatePageAccessItems(extension, web_contents);
    AddSeparator(ui::NORMAL_SEPARATOR);
  }

  if (OptionsPageInfo::HasOptionsPage(extension))
    AddItemWithStringId(OPTIONS, IDS_EXTENSIONS_OPTIONS_MENU_ITEM);

  if (!is_component_) {
    if (IsExtensionRequiredByPolicy(extension, profile_)) {
      // TODO (kylixrd): Investigate the usage of the hard-coded color.
      AddItemWithStringIdAndIcon(
          POLICY_INSTALLED, IDS_EXTENSIONS_INSTALLED_BY_ADMIN,
          ui::ImageModel::FromVectorIcon(vector_icons::kBusinessIcon,
                                         ui::kColorIcon, 16));

    } else {
      AddItemWithStringId(UNINSTALL, IDS_EXTENSIONS_UNINSTALL);
    }
  }

  if (can_show_icon_in_toolbar &&
      source_ == ContextMenuSource::kToolbarAction) {
    int visibility_string_id =
        GetVisibilityStringId(profile_, extension, is_pinned_);
    DCHECK_NE(-1, visibility_string_id);
    AddItemWithStringId(TOGGLE_VISIBILITY, visibility_string_id);
    if (IsExtensionForcePinned(*extension, profile_)) {
      size_t toggle_visibility_index =
          GetIndexOfCommandId(TOGGLE_VISIBILITY).value();
      SetIcon(toggle_visibility_index,
              ui::ImageModel::FromVectorIcon(vector_icons::kBusinessIcon,
                                             ui::kColorIcon, 16));
    }
  }

  AddSidePanelEntryIfPresent(*extension);

  if (!is_component_) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItemWithStringId(MANAGE_EXTENSIONS, IDS_MANAGE_EXTENSION);
    AddItemWithStringId(VIEW_WEB_PERMISSIONS, IDS_VIEW_WEB_PERMISSIONS);
  }

  const ActionInfo* action_info = ActionInfo::GetExtensionActionInfo(extension);
  if (delegate_ && !is_component_ && action_info && !action_info->synthesized &&
      profile_->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode)) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItemWithStringId(INSPECT_POPUP, IDS_EXTENSION_ACTION_INSPECT_POPUP);
  }
}

void ExtensionContextMenuModel::AddSidePanelEntryIfPresent(
    const Extension& extension) {
  if (!extension.permissions_data()->HasAPIPermission(
          mojom::APIPermissionID::kSidePanel)) {
    return;
  }

  SidePanelService* const side_panel_service = GetSidePanelService();
  CHECK(side_panel_service);

  int tab_id = ExtensionTabUtil::GetTabId(GetActiveWebContents());
  if (!side_panel_service->HasSidePanelContextMenuActionForTab(extension,
                                                               tab_id)) {
    return;
  }

  AddSeparator(ui::NORMAL_SEPARATOR);
  SidePanelUI* const side_panel_ui = browser_->GetFeatures().side_panel_ui();
  CHECK(side_panel_ui);
  bool is_side_panel_open = side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kExtension, extension.id()));
  AddItemWithStringId(TOGGLE_SIDE_PANEL_VISIBILITY,
                      is_side_panel_open
                          ? IDS_EXTENSIONS_SUBMENU_CLOSE_SIDE_PANEL_ITEM
                          : IDS_EXTENSIONS_SUBMENU_OPEN_SIDE_PANEL_ITEM);
}

const Extension* ExtensionContextMenuModel::GetExtension() const {
  return ExtensionRegistry::Get(profile_)->enabled_extensions().GetByID(
      extension_id_);
}

void ExtensionContextMenuModel::AppendExtensionItems() {
  MenuManager* menu_manager = MenuManager::Get(profile_);
  if (!menu_manager ||  // Null in unit tests
      !menu_manager->MenuItems(MenuItem::ExtensionKey(extension_id_)))
    return;

  AddSeparator(ui::NORMAL_SEPARATOR);

  int index = 0;
  extension_items_->AppendExtensionItems(MenuItem::ExtensionKey(extension_id_),
                                         std::u16string(), &index,
                                         true);  // is_action_menu
}

void ExtensionContextMenuModel::CreatePageAccessItems(
    const Extension* extension,
    content::WebContents* web_contents) {
  DCHECK(!base::FeatureList::IsEnabled(
      extensions_features::kExtensionsMenuAccessControl));

  const GURL& url = web_contents->GetLastCommittedURL();
  auto* permissions_manager = PermissionsManager::Get(profile_);

  // The extension wants site access but can't run on the page if it does
  // not have at least "on click" access.
  if (!permissions_manager->CanUserSelectSiteAccess(
          *extension, url, PermissionsManager::UserSiteAccess::kOnClick)) {
    AddItemWithStringId(PAGE_ACCESS_CANT_ACCESS,
                        IDS_EXTENSIONS_CONTEXT_MENU_CANT_ACCESS_PAGE);
    return;
  }

  // The extension wants site access and can ran on the page.  Add the three
  // options for "on click", "on this site", "on all sites". Though we
  // always add these three, some may be disabled.
  static constexpr int kRadioGroup = 0;
  page_access_submenu_ = std::make_unique<ui::SimpleMenuModel>(this);

  page_access_submenu_->AddRadioItemWithStringId(
      PAGE_ACCESS_RUN_ON_CLICK,
      IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_RUN_ON_CLICK, kRadioGroup);
  page_access_submenu_->AddRadioItem(
      PAGE_ACCESS_RUN_ON_SITE,
      l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_RUN_ON_SITE,
          GetCurrentSite(url)),
      kRadioGroup);
  page_access_submenu_->AddRadioItemWithStringId(
      PAGE_ACCESS_RUN_ON_ALL_SITES,
      IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_RUN_ON_ALL_SITES, kRadioGroup);

  page_access_submenu_->AddSeparator(ui::NORMAL_SEPARATOR);
  page_access_submenu_->AddItemWithStringId(
      PAGE_ACCESS_LEARN_MORE,
      IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_LEARN_MORE);

  AddSubMenuWithStringId(PAGE_ACCESS_SUBMENU,
                         IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS,
                         page_access_submenu_.get());
}

content::WebContents* ExtensionContextMenuModel::GetActiveWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

SidePanelService* ExtensionContextMenuModel::GetSidePanelService() const {
  return SidePanelService::Get(profile_);
}

}  // namespace extensions
