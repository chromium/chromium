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
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/chrome_extension_browser_constants.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/site_permissions_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
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
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"

namespace extensions {

namespace {

// Returns true if the given |item| is of the given |type|.
bool MenuItemMatchesAction(const absl::optional<ActionInfo::Type> action_type,
                           const MenuItem* item) {
  if (!action_type)
    return false;

  const MenuItem::ContextList& contexts = item->contexts();

  if (contexts.Contains(MenuItem::ALL))
    return true;
  if (contexts.Contains(MenuItem::PAGE_ACTION) &&
      (*action_type == ActionInfo::TYPE_PAGE)) {
    return true;
  }
  if (contexts.Contains(MenuItem::BROWSER_ACTION) &&
      (*action_type == ActionInfo::TYPE_BROWSER)) {
    return true;
  }
  if (contexts.Contains(MenuItem::ACTION) &&
      (*action_type == ActionInfo::TYPE_ACTION)) {
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
int GetVisibilityStringId(
    Profile* profile,
    const Extension* extension,
    ExtensionContextMenuModel::ButtonVisibility button_visibility) {
  if (IsExtensionForcePinned(*extension, profile))
    return IDS_EXTENSIONS_PINNED_BY_ADMIN;
  if (button_visibility == ExtensionContextMenuModel::PINNED)
    return IDS_EXTENSIONS_UNPIN_FROM_TOOLBAR;
  return IDS_EXTENSIONS_PIN_TO_TOOLBAR;
}

// Returns true if the given |extension| is required to remain installed by
// policy.
bool IsExtensionRequiredByPolicy(const Extension* extension, Profile* profile) {
  ManagementPolicy* policy = ExtensionSystem::Get(profile)->management_policy();
  return !policy->UserMayModifySettings(extension, nullptr) ||
         policy->MustRemainInstalled(extension, nullptr);
}

std::u16string GetCurrentSite(content::WebContents* web_contents) {
  return url_formatter::IDNToUnicode(
      url_formatter::StripWWW(web_contents->GetLastCommittedURL().host()));
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
      NOTREACHED();
      break;
    case ExtensionContextMenuModel::VIEW_WEB_PERMISSIONS:
      return ContextMenuAction::kViewWebPermissions;
    default:
      break;
  }
  NOTREACHED();
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
  NOTREACHED();
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
      NOTREACHED() << "Unknown option: " << command_id;
      break;
  }
}

void OpenUrl(Browser& browser, const GURL& url) {
  content::OpenURLParams params(
      url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false);
  browser.OpenURL(params);
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
  UninstallDialogHelper() {}
  ~UninstallDialogHelper() override {}

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
    ButtonVisibility button_visibility,
    PopupDelegate* delegate,
    bool can_show_icon_in_toolbar,
    ContextMenuSource source)
    : SimpleMenuModel(this),
      extension_id_(extension->id()),
      is_component_(Manifest::IsComponentLocation(extension->location())),
      browser_(browser),
      profile_(browser->profile()),
      delegate_(delegate),
      button_visibility_(button_visibility),
      source_(source) {
  InitMenu(extension, can_show_icon_in_toolbar);
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
    content::WebContents* web_contents = GetActiveWebContents();
    if (!web_contents)
      return false;

    auto* permissions = PermissionsManager::Get(profile_);
    PermissionsManager::UserSiteAccess current_access =
        permissions->GetUserSiteAccess(*extension,
                                       web_contents->GetLastCommittedURL());
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
      return !IsExtensionRequiredByPolicy(extension, profile_);
    case PAGE_ACCESS_CANT_ACCESS:
    case PAGE_ACCESS_ALL_EXTENSIONS_GRANTED:
    case PAGE_ACCESS_ALL_EXTENSIONS_BLOCKED:
    case PAGE_ACCESS_SUBMENU:
    case PAGE_ACCESS_RUN_ON_CLICK:
    case PAGE_ACCESS_RUN_ON_SITE:
    case PAGE_ACCESS_RUN_ON_ALL_SITES:
    case PAGE_ACCESS_PERMISSIONS_PAGE:
    case PAGE_ACCESS_LEARN_MORE: {
      return IsPageAccessCommandEnabled(*extension, command_id);
    }
    // Extension pinning/unpinning is not available for Incognito as this leaves
    // a trace of user activity.
    case TOGGLE_VISIBILITY:
      return !browser_->profile()->IsOffTheRecord() &&
             !IsExtensionForcePinned(*extension, profile_);
    // Manage extensions and view web permissions are always enabled.
    case MANAGE_EXTENSIONS:
    case VIEW_WEB_PERMISSIONS:
      return true;
    default:
      NOTREACHED() << "Unknown command" << command_id;
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
      bool currently_visible = button_visibility_ == PINNED;
      ToolbarActionsModel::Get(browser_->profile())
          ->SetActionVisibility(extension->id(), !currently_visible);
      break;
    }
    case UNINSTALL: {
      UninstallDialogHelper::UninstallExtension(browser_, extension);
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
    case PAGE_ACCESS_RUN_ON_CLICK:
    case PAGE_ACCESS_RUN_ON_SITE:
    case PAGE_ACCESS_RUN_ON_ALL_SITES:
    case PAGE_ACCESS_PERMISSIONS_PAGE:
    case PAGE_ACCESS_LEARN_MORE:
      HandlePageAccessCommand(command_id, extension);
      break;
    default:
      NOTREACHED() << "Unknown option";
      break;
  }
}

void ExtensionContextMenuModel::OnMenuWillShow(ui::SimpleMenuModel* menu) {
  action_taken_ = ContextMenuAction::kNoAction;
}

void ExtensionContextMenuModel::MenuClosed(ui::SimpleMenuModel* menu) {
  if (action_taken_) {
    ContextMenuAction action = *action_taken_;
    UMA_HISTOGRAM_ENUMERATION("Extensions.ContextMenuAction", action);
    action_taken_ = absl::nullopt;
  }
}

ExtensionContextMenuModel::~ExtensionContextMenuModel() {}

void ExtensionContextMenuModel::InitMenu(const Extension* extension,
                                         bool can_show_icon_in_toolbar) {
  DCHECK(extension);

  absl::optional<ActionInfo::Type> action_type;
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
    bool is_required_by_policy =
        IsExtensionRequiredByPolicy(extension, profile_);
    int message_id = is_required_by_policy ? IDS_EXTENSIONS_INSTALLED_BY_ADMIN
                                           : IDS_EXTENSIONS_UNINSTALL;
    AddItem(UNINSTALL, l10n_util::GetStringUTF16(message_id));
    if (is_required_by_policy) {
      size_t uninstall_index = GetIndexOfCommandId(UNINSTALL).value();
      // TODO (kylixrd): Investigate the usage of the hard-coded color.
      SetIcon(uninstall_index,
              ui::ImageModel::FromVectorIcon(vector_icons::kBusinessIcon,
                                             ui::kColorIcon, 16));
    }
  }

  if ((source_ == ContextMenuSource::kToolbarAction) &&
      can_show_icon_in_toolbar) {
    int visibility_string_id =
        GetVisibilityStringId(profile_, extension, button_visibility_);
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

bool ExtensionContextMenuModel::IsPageAccessCommandEnabled(
    const Extension& extension,
    int command_id) const {
  content::WebContents* web_contents = GetActiveWebContents();
  if (!web_contents)
    return false;

  switch (command_id) {
    case PAGE_ACCESS_CANT_ACCESS:
    case PAGE_ACCESS_ALL_EXTENSIONS_GRANTED:
    case PAGE_ACCESS_ALL_EXTENSIONS_BLOCKED:
      // When these commands are shown, they are always disabled.
      return false;

    case PAGE_ACCESS_SUBMENU:
    case PAGE_ACCESS_LEARN_MORE:
    case PAGE_ACCESS_PERMISSIONS_PAGE:
      // When these commands are shown, they are always enabled.
      return true;

    case PAGE_ACCESS_RUN_ON_CLICK:
    case PAGE_ACCESS_RUN_ON_SITE:
    case PAGE_ACCESS_RUN_ON_ALL_SITES:
      // Verify the extension wants access to the page - that's the only time
      // these commands should be shown.
      const GURL& url = web_contents->GetLastCommittedURL();
      auto* permissions_manager = PermissionsManager::Get(profile_);
      DCHECK(permissions_manager->HasActiveTabAndCanAccess(extension, url) ||
             permissions_manager->CanAffectExtension(extension) &&
                 permissions_manager->CanUserSelectSiteAccess(
                     extension, url,
                     PermissionsManager::UserSiteAccess::kOnClick));

      // TODO(devlin): This can lead to some fun race-like conditions, where the
      // menu is constructed during navigation. Since we get the URL both here
      // and in execution of the command, there's a chance we'll find two
      // different URLs. This would be solved if we maintained the URL that the
      // menu was showing for.
      return permissions_manager->CanUserSelectSiteAccess(
          extension, url, CommandIdToSiteAccess(command_id));
  }

  NOTREACHED() << "Unexpected command id: " << command_id;
  return false;
}

void ExtensionContextMenuModel::CreatePageAccessItems(
    const Extension* extension,
    content::WebContents* web_contents) {
  auto url = web_contents->GetLastCommittedURL();
  auto* permissions_manager = PermissionsManager::Get(profile_);

  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    auto add_page_access_secondary_buttons = [](ui::SimpleMenuModel* parent) {
      parent->AddSeparator(ui::NORMAL_SEPARATOR);
      parent->AddItemWithStringId(
          PAGE_ACCESS_PERMISSIONS_PAGE,
          IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_PERMISSIONS_PAGE);
      parent->AddItemWithStringId(
          PAGE_ACCESS_LEARN_MORE,
          IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_LEARN_MORE);
    };

    // User site setting takes preference over extension settings. Therefore, we
    // only show the page access submenu with change extension settings options
    // if the site settings is set to "customize by extension". Otherwise, shows
    // a message that informs the user about the site setting.
    auto site_setting =
        permissions_manager->GetUserSiteSetting(url::Origin::Create(url));
    switch (site_setting) {
      case PermissionsManager::UserSiteSetting::kGrantAllExtensions:
        AddItem(
            PAGE_ACCESS_ALL_EXTENSIONS_GRANTED,
            l10n_util::GetStringFUTF16(
                IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_ALL_EXTENSIONS_GRANTED,
                GetCurrentSite(web_contents)));
        add_page_access_secondary_buttons(this);
        return;

      case PermissionsManager::UserSiteSetting::kBlockAllExtensions:
        AddItem(
            PAGE_ACCESS_ALL_EXTENSIONS_BLOCKED,
            l10n_util::GetStringFUTF16(
                IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_ALL_EXTENSIONS_BLOCKED,
                GetCurrentSite(web_contents)));
        add_page_access_secondary_buttons(this);
        return;

      case PermissionsManager::UserSiteSetting::kCustomizeByExtension:
        // The extension wants site access but cant't run on the page if it does
        // not have at least "on click" access.
        if (!permissions_manager->CanUserSelectSiteAccess(
                *extension, url,
                PermissionsManager::UserSiteAccess::kOnClick)) {
          AddItemWithStringId(PAGE_ACCESS_CANT_ACCESS,
                              IDS_EXTENSIONS_CONTEXT_MENU_CANT_ACCESS_PAGE);
          return;
        }

        // The extension wants site access and can ran on the page.  Add the
        // three options for "on click", "on this site", "on all sites". Though
        // we always add these three, some may be disabled.
        const int kRadioGroup = 0;
        page_access_submenu_ = std::make_unique<ui::SimpleMenuModel>(this);
        page_access_submenu_->AddRadioItemWithStringId(
            PAGE_ACCESS_RUN_ON_CLICK,
            IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_RUN_ON_CLICK_V2,
            kRadioGroup);
        page_access_submenu_->AddRadioItem(
            PAGE_ACCESS_RUN_ON_SITE,
            l10n_util::GetStringFUTF16(
                IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_RUN_ON_SITE_V2,
                GetCurrentSite(web_contents)),
            kRadioGroup);
        page_access_submenu_->AddRadioItemWithStringId(
            PAGE_ACCESS_RUN_ON_ALL_SITES,
            IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_RUN_ON_ALL_SITES_V2,
            kRadioGroup);
        add_page_access_secondary_buttons(page_access_submenu_.get());

        AddSubMenuWithStringId(PAGE_ACCESS_SUBMENU,
                               IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS,
                               page_access_submenu_.get());
    }
  } else {
    // The extension wants site access but cant't run on the page if it does
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
    const int kRadioGroup = 0;
    page_access_submenu_ = std::make_unique<ui::SimpleMenuModel>(this);

    page_access_submenu_->AddRadioItemWithStringId(
        PAGE_ACCESS_RUN_ON_CLICK,
        IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_RUN_ON_CLICK, kRadioGroup);
    page_access_submenu_->AddRadioItem(
        PAGE_ACCESS_RUN_ON_SITE,
        l10n_util::GetStringFUTF16(
            IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_RUN_ON_SITE,
            GetCurrentSite(web_contents)),
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
}

void ExtensionContextMenuModel::HandlePageAccessCommand(
    int command_id,
    const Extension* extension) const {
  content::WebContents* web_contents = GetActiveWebContents();
  if (!web_contents)
    return;

  LogPageAccessAction(command_id);

  if (command_id == PAGE_ACCESS_PERMISSIONS_PAGE) {
    OpenUrl(*browser_,
            GURL(chrome_extension_constants::kExtensionsSitePermissionsURL));
    return;
  }
  if (command_id == PAGE_ACCESS_LEARN_MORE) {
    OpenUrl(*browser_,
            GURL(chrome_extension_constants::kRuntimeHostPermissionsHelpURL));
    return;
  }

  SitePermissionsHelper permissions(profile_);
  permissions.UpdateSiteAccess(*extension, web_contents,
                               CommandIdToSiteAccess(command_id));
}

content::WebContents* ExtensionContextMenuModel::GetActiveWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

}  // namespace extensions
