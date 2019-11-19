// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_context_menu_model.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/chrome_extension_browser_constants.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_formatter.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/context_menu_params.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/manifest_url_handlers.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"

namespace extensions {

namespace {

// Returns true if the given |item| is of the given |type|.
bool MenuItemMatchesAction(ExtensionContextMenuModel::ActionType type,
                           const MenuItem* item) {
  if (type == ExtensionContextMenuModel::NO_ACTION)
    return false;

  const MenuItem::ContextList& contexts = item->contexts();

  if (contexts.Contains(MenuItem::ALL))
    return true;
  if (contexts.Contains(MenuItem::PAGE_ACTION) &&
      (type == ExtensionContextMenuModel::PAGE_ACTION))
    return true;
  if (contexts.Contains(MenuItem::BROWSER_ACTION) &&
      (type == ExtensionContextMenuModel::BROWSER_ACTION))
    return true;

  return false;
}

// Returns the id for the visibility command for the given |extension|.
int GetVisibilityStringId(
    Profile* profile,
    const Extension* extension,
    ExtensionContextMenuModel::ButtonVisibility button_visibility) {
  if (base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu)) {
    return button_visibility == ExtensionContextMenuModel::VISIBLE
               ? IDS_EXTENSIONS_UNPIN_FROM_TOOLBAR
               : IDS_EXTENSIONS_PIN_TO_TOOLBAR;
  }
  DCHECK(profile);
  int string_id = -1;
  // We display "show" or "hide" based on the icon's visibility, and can have
  // "transitively shown" buttons that are shown only while the button has a
  // popup or menu visible.
  switch (button_visibility) {
    case (ExtensionContextMenuModel::VISIBLE):
      string_id = IDS_EXTENSIONS_HIDE_BUTTON_IN_MENU;
      break;
    case (ExtensionContextMenuModel::TRANSITIVELY_VISIBLE):
      string_id = IDS_EXTENSIONS_KEEP_BUTTON_IN_TOOLBAR;
      break;
    case (ExtensionContextMenuModel::OVERFLOWED):
      string_id = IDS_EXTENSIONS_SHOW_BUTTON_IN_TOOLBAR;
      break;
  }

  return string_id;
}

// Returns true if the given |extension| is required to remain installed by
// policy.
bool IsExtensionRequiredByPolicy(const Extension* extension,
                                 Profile* profile) {
  ManagementPolicy* policy = ExtensionSystem::Get(profile)->management_policy();
  return !policy->UserMayModifySettings(extension, nullptr) ||
         policy->MustRemainInstalled(extension, nullptr);
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
    case ExtensionContextMenuModel::PAGE_ACCESS_LEARN_MORE:
      return ContextMenuAction::kPageAccessLearnMore;
    case ExtensionContextMenuModel::PAGE_ACCESS_CANT_ACCESS:
    case ExtensionContextMenuModel::PAGE_ACCESS_SUBMENU:
      NOTREACHED();
      break;
    default:
      break;
  }
  NOTREACHED();
  return ContextMenuAction::kNoAction;
}

// A stub for the uninstall dialog.
// TODO(devlin): Ideally, we would just have the uninstall dialog take a
// base::Callback, but that's a bunch of churn.
class UninstallDialogHelper : public ExtensionUninstallDialog::Delegate {
 public:
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
                                        const base::string16& error) override {
    delete this;
  }

  std::unique_ptr<ExtensionUninstallDialog> uninstall_dialog_;

  DISALLOW_COPY_AND_ASSIGN(UninstallDialogHelper);
};

}  // namespace

ExtensionContextMenuModel::ExtensionContextMenuModel(
    const Extension* extension,
    Browser* browser,
    ButtonVisibility button_visibility,
    PopupDelegate* delegate,
    bool can_show_icon_in_toolbar)
    : SimpleMenuModel(this),
      extension_id_(extension->id()),
      is_component_(Manifest::IsComponentLocation(extension->location())),
      browser_(browser),
      profile_(browser->profile()),
      delegate_(delegate),
      action_type_(NO_ACTION),
      button_visibility_(button_visibility),
      can_show_icon_in_toolbar_(can_show_icon_in_toolbar) {
  InitMenu(extension, button_visibility);
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
    return web_contents &&
           GetCurrentPageAccess(extension, web_contents) == command_id;
  }

  return false;
}

bool ExtensionContextMenuModel::IsCommandIdVisible(int command_id) const {
  const Extension* extension = GetExtension();
  if (!extension)
    return false;
  if (ContextMenuMatcher::IsExtensionsCustomCommandId(command_id)) {
    return extension_items_->IsCommandIdVisible(command_id);
  }

  // The command is hidden in app windows because they don't
  // support showing extensions in the app window frame.
  if (command_id == TOGGLE_VISIBILITY)
    return can_show_icon_in_toolbar_;

  // Standard menu items are visible.
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
      return OptionsPageInfo::HasOptionsPage(extension);
    case INSPECT_POPUP: {
      content::WebContents* web_contents = GetActiveWebContents();
      return web_contents && extension_action_ &&
             extension_action_->HasPopup(
                 SessionTabHelper::IdForTab(web_contents).id());
    }
    case UNINSTALL:
      return !IsExtensionRequiredByPolicy(extension, profile_);
    case PAGE_ACCESS_CANT_ACCESS:
    case PAGE_ACCESS_SUBMENU:
    case PAGE_ACCESS_RUN_ON_CLICK:
    case PAGE_ACCESS_RUN_ON_SITE:
    case PAGE_ACCESS_RUN_ON_ALL_SITES:
    case PAGE_ACCESS_LEARN_MORE: {
      content::WebContents* web_contents = GetActiveWebContents();
      if (!web_contents)
        return false;
      // TODO(devlin): This can lead to some fun race-like conditions, where the
      // menu is constructed during navigation. Since we get the URL both here
      // and in execution of the command, there's a chance we'll find two
      // different URLs. This would be solved if we maintained the URL that the
      // menu was showing for.
      const GURL& url = web_contents->GetLastCommittedURL();
      return IsPageAccessCommandEnabled(*extension, url, command_id);
    }
    // The following, if they are present, are always enabled.
    case TOGGLE_VISIBILITY:
    case MANAGE_EXTENSIONS:
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
      content::OpenURLParams params(ManifestURL::GetHomepageURL(extension),
                                    content::Referrer(),
                                    WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                    ui::PAGE_TRANSITION_LINK, false);
      browser_->OpenURL(params);
      break;
    }
    case OPTIONS:
      DCHECK(OptionsPageInfo::HasOptionsPage(extension));
      ExtensionTabUtil::OpenOptionsPage(extension, browser_);
      break;
    case TOGGLE_VISIBILITY: {
      bool currently_visible = button_visibility_ == VISIBLE;
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
    case INSPECT_POPUP: {
      delegate_->InspectPopup();
      break;
    }
    case PAGE_ACCESS_RUN_ON_CLICK:
    case PAGE_ACCESS_RUN_ON_SITE:
    case PAGE_ACCESS_RUN_ON_ALL_SITES:
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
    action_taken_ = base::nullopt;
  }
}

ExtensionContextMenuModel::~ExtensionContextMenuModel() {}

void ExtensionContextMenuModel::InitMenu(const Extension* extension,
                                         ButtonVisibility button_visibility) {
  DCHECK(extension);

  extension_action_ =
      ExtensionActionManager::Get(profile_)->GetExtensionAction(*extension);
  if (extension_action_) {
    action_type_ = extension_action_->action_type() == ActionInfo::TYPE_PAGE
                       ? PAGE_ACTION
                       : BROWSER_ACTION;
  }

  extension_items_.reset(new ContextMenuMatcher(
      profile_, this, this, base::Bind(MenuItemMatchesAction, action_type_)));

  std::string extension_name = extension->name();
  // Ampersands need to be escaped to avoid being treated like
  // mnemonics in the menu.
  base::ReplaceChars(extension_name, "&", "&&", &extension_name);
  AddItem(HOME_PAGE, base::UTF8ToUTF16(extension_name));
  AppendExtensionItems();
  AddSeparator(ui::NORMAL_SEPARATOR);

  CreatePageAccessSubmenu(extension);

  if (!is_component_ || OptionsPageInfo::HasOptionsPage(extension))
    AddItemWithStringId(OPTIONS, IDS_EXTENSIONS_OPTIONS_MENU_ITEM);

  if (!is_component_) {
    bool is_required_by_policy =
        IsExtensionRequiredByPolicy(extension, profile_);
    int message_id = is_required_by_policy ?
        IDS_EXTENSIONS_INSTALLED_BY_ADMIN : IDS_EXTENSIONS_UNINSTALL;
    AddItem(UNINSTALL, l10n_util::GetStringUTF16(message_id));
    if (is_required_by_policy) {
      int uninstall_index = GetIndexOfCommandId(UNINSTALL);
      SetIcon(uninstall_index,
              gfx::Image(gfx::CreateVectorIcon(vector_icons::kBusinessIcon, 16,
                                               gfx::kChromeIconGrey)));
    }
  }

  // Add a toggle visibility (show/hide) if the extension icon is shown on the
  // toolbar.
  int visibility_string_id =
      GetVisibilityStringId(profile_, extension, button_visibility);
  DCHECK_NE(-1, visibility_string_id);
  AddItemWithStringId(TOGGLE_VISIBILITY, visibility_string_id);

  if (!is_component_) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItemWithStringId(MANAGE_EXTENSIONS, IDS_MANAGE_EXTENSION);
  }

  const ActionInfo* action_info = ActionInfo::GetAnyActionInfo(extension);
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
                                         base::string16(), &index,
                                         true);  // is_action_menu
}

ExtensionContextMenuModel::MenuEntries
ExtensionContextMenuModel::GetCurrentPageAccess(
    const Extension* extension,
    content::WebContents* web_contents) const {
  DCHECK(web_contents);
  ScriptingPermissionsModifier modifier(profile_, extension);
  DCHECK(modifier.CanAffectExtension());
  ScriptingPermissionsModifier::SiteAccess site_access =
      modifier.GetSiteAccess(web_contents->GetLastCommittedURL());
  if (site_access.has_all_sites_access)
    return PAGE_ACCESS_RUN_ON_ALL_SITES;
  if (site_access.has_site_access)
    return PAGE_ACCESS_RUN_ON_SITE;
  return PAGE_ACCESS_RUN_ON_CLICK;
}

bool ExtensionContextMenuModel::IsPageAccessCommandEnabled(
    const Extension& extension,
    const GURL& url,
    int command_id) const {
  // The "Can't access this site" entry is, by design, always disabled.
  if (command_id == PAGE_ACCESS_CANT_ACCESS)
    return false;

  ScriptingPermissionsModifier modifier(profile_, &extension);
  DCHECK(modifier.CanAffectExtension());

  ScriptingPermissionsModifier::SiteAccess site_access =
      modifier.GetSiteAccess(url);

  // Verify the extension wants access to the page - that's the only time these
  // commands should be shown.
  DCHECK(site_access.has_site_access || site_access.withheld_site_access ||
         extension.permissions_data()->HasAPIPermission(
             APIPermission::kActiveTab));

  switch (command_id) {
    case PAGE_ACCESS_SUBMENU:
    case PAGE_ACCESS_LEARN_MORE:
    case PAGE_ACCESS_RUN_ON_CLICK:
      // These are always enabled.
      return true;
    case PAGE_ACCESS_RUN_ON_SITE:
      // The "on this site" option is only enabled if the extension wants to
      // always run on the site without user interaction.
      return site_access.has_site_access || site_access.withheld_site_access;
    case PAGE_ACCESS_RUN_ON_ALL_SITES:
      // The "on all sites" option is only enabled if the extension wants to be
      // able to run everywhere.
      return site_access.has_all_sites_access ||
             site_access.withheld_all_sites_access;
    default:
      break;
  }

  NOTREACHED() << "Unexpected command id: " << command_id;
  return false;
}

void ExtensionContextMenuModel::CreatePageAccessSubmenu(
    const Extension* extension) {
  content::WebContents* web_contents = GetActiveWebContents();
  if (!web_contents)
    return;

  ScriptingPermissionsModifier modifier(profile_, extension);
  if (!modifier.CanAffectExtension())
    return;

  const GURL& url = web_contents->GetLastCommittedURL();
  ScriptingPermissionsModifier::SiteAccess site_access =
      modifier.GetSiteAccess(url);

  bool has_active_tab = extension->permissions_data()->HasAPIPermission(
      APIPermission::kActiveTab);
  bool wants_site_access =
      site_access.has_site_access || site_access.withheld_site_access;
  if (!wants_site_access && !has_active_tab) {
    AddItemWithStringId(PAGE_ACCESS_CANT_ACCESS,
                        IDS_EXTENSIONS_CONTEXT_MENU_CANT_ACCESS_PAGE);
    return;
  }

  const int kRadioGroup = 0;
  page_access_submenu_ = std::make_unique<ui::SimpleMenuModel>(this);

  // Add the three options for "on click", "on this site", "on all sites".
  // Though we always add these three, some may be disabled.
  page_access_submenu_->AddRadioItemWithStringId(
      PAGE_ACCESS_RUN_ON_CLICK,
      IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_RUN_ON_CLICK, kRadioGroup);
  page_access_submenu_->AddRadioItem(
      PAGE_ACCESS_RUN_ON_SITE,
      l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_RUN_ON_SITE,
          url_formatter::StripWWW(base::UTF8ToUTF16(
              url::Origin::Create(web_contents->GetLastCommittedURL())
                  .host()))),
      kRadioGroup);
  page_access_submenu_->AddRadioItemWithStringId(
      PAGE_ACCESS_RUN_ON_ALL_SITES,
      IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_RUN_ON_ALL_SITES, kRadioGroup);

  // Add the learn more link.
  page_access_submenu_->AddSeparator(ui::NORMAL_SEPARATOR);
  page_access_submenu_->AddItemWithStringId(
      PAGE_ACCESS_LEARN_MORE,
      IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_LEARN_MORE);

  AddSubMenuWithStringId(PAGE_ACCESS_SUBMENU,
                         IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS,
                         page_access_submenu_.get());
}

void ExtensionContextMenuModel::HandlePageAccessCommand(
    int command_id,
    const Extension* extension) const {
  content::WebContents* web_contents = GetActiveWebContents();
  if (!web_contents)
    return;

  if (command_id == PAGE_ACCESS_LEARN_MORE) {
    content::OpenURLParams params(
        GURL(chrome_extension_constants::kRuntimeHostPermissionsHelpURL),
        content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui::PAGE_TRANSITION_LINK, false);
    browser_->OpenURL(params);
    return;
  }

  MenuEntries current_access = GetCurrentPageAccess(extension, web_contents);
  if (command_id == current_access)
    return;

  auto convert_page_access = [](int command_id) {
    switch (command_id) {
      case PAGE_ACCESS_RUN_ON_CLICK:
        return ExtensionActionRunner::PageAccess::RUN_ON_CLICK;
      case PAGE_ACCESS_RUN_ON_SITE:
        return ExtensionActionRunner::PageAccess::RUN_ON_SITE;
      case PAGE_ACCESS_RUN_ON_ALL_SITES:
        return ExtensionActionRunner::PageAccess::RUN_ON_ALL_SITES;
    }
    NOTREACHED();
    return ExtensionActionRunner::PageAccess::RUN_ON_CLICK;
  };

  ExtensionActionRunner* runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  if (runner)
    runner->HandlePageAccessModified(extension,
                                     convert_page_access(current_access),
                                     convert_page_access(command_id));
}

content::WebContents* ExtensionContextMenuModel::GetActiveWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

}  // namespace extensions
