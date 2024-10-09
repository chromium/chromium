// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tab_util.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <utility>

#include "base/containers/fixed_flat_set.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/url_constants.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

using content::NavigationEntry;
using content::WebContents;
using extensions::mojom::APIPermissionID;

namespace extensions {

namespace {

constexpr char kGroupNotFoundError[] = "No group with id: *.";
constexpr char kInvalidUrlError[] = "Invalid url: \"*\".";

// This enum is used for counting schemes used via a navigation triggered by
// extensions.
enum class NavigationScheme {
  // http: or https: scheme.
  kHttpOrHttps = 0,
  // chrome: scheme.
  kChrome = 1,
  // file: scheme where extension has access to local files.
  kFileWithPermission = 2,
  // file: scheme where extension does NOT have access to local files.
  kFileWithoutPermission = 3,
  // Everything else.
  kOther = 4,

  kMaxValue = kOther,
};

// TODO(b/361838438) Remove this. The code should consistently use
// ExtensionBrowserWindow but during the transition code uses both that and
// Browser* and we need to convert between the two.
//
// Guaranteed non-null when the extensions system is still attached to the
// Browser (callers shouldn't need to null check).
WindowController* WindowControllerFromBrowser(const Browser* browser) {
  return browser->extension_window_controller();
}

Browser* CreateBrowser(Profile* profile, bool user_gesture) {
  if (Browser::GetCreationStatusForProfile(profile) !=
      Browser::CreationStatus::kOk) {
    return nullptr;
  }

  Browser::CreateParams params(Browser::TYPE_NORMAL, profile, user_gesture);
  return Browser::Create(params);
}

Browser* CreateAndShowBrowser(Profile* profile,
                              bool user_gesture,
                              std::string* error) {
  Browser* browser = CreateBrowser(profile, user_gesture);
  if (!browser) {
    *error = ExtensionTabUtil::kBrowserWindowNotAllowed;
    return nullptr;
  }
  browser->window()->Show();
  return browser;
}

// Use this function for reporting a tab id to an extension. It will
// take care of setting the id to TAB_ID_NONE if necessary (for
// example with devtools).
int GetTabIdForExtensions(const WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (browser && !ExtensionTabUtil::BrowserSupportsTabs(browser))
    return -1;
  return sessions::SessionTabHelper::IdForTab(web_contents).id();
}

bool IsFileUrl(const GURL& url) {
  return url.SchemeIsFile() || (url.SchemeIs(content::kViewSourceScheme) &&
                                GURL(url.GetContent()).SchemeIsFile());
}

ExtensionTabUtil::ScrubTabBehaviorType GetScrubTabBehaviorImpl(
    const Extension* extension,
    mojom::ContextType context,
    const GURL& url,
    int tab_id) {
  if (context == mojom::ContextType::kWebUi) {
    return ExtensionTabUtil::kDontScrubTab;
  }

  if (context == mojom::ContextType::kUntrustedWebUi) {
    return ExtensionTabUtil::kScrubTabFully;
  }

  bool has_permission = false;

  if (extension) {
    bool api_permission = false;
    if (tab_id == api::tabs::TAB_ID_NONE) {
      api_permission = extension->permissions_data()->HasAPIPermission(
          APIPermissionID::kTab);
    } else {
      api_permission = extension->permissions_data()->HasAPIPermissionForTab(
          tab_id, APIPermissionID::kTab);
    }

    bool host_permission = extension->permissions_data()
                               ->active_permissions()
                               .HasExplicitAccessToOrigin(url);
    has_permission = api_permission || host_permission;
  }

  if (!has_permission) {
    return ExtensionTabUtil::kScrubTabFully;
  }

  return ExtensionTabUtil::kDontScrubTab;
}

bool HasValidMainFrameProcess(content::WebContents* contents) {
  content::RenderFrameHost* main_frame_host = contents->GetPrimaryMainFrame();
  content::RenderProcessHost* process_host = main_frame_host->GetProcess();
  return process_host->IsReady() && process_host->IsInitializedAndNotDead();
}

void RecordNavigationScheme(const GURL& url,
                            const Extension& extension,
                            content::BrowserContext* browser_context) {
  NavigationScheme scheme = NavigationScheme::kOther;

  if (url.SchemeIsHTTPOrHTTPS()) {
    scheme = NavigationScheme::kHttpOrHttps;
  } else if (url.SchemeIs(content::kChromeUIScheme)) {
    scheme = NavigationScheme::kChrome;
  } else if (url.SchemeIsFile()) {
    scheme = (util::AllowFileAccess(extension.id(), browser_context))
                 ? NavigationScheme::kFileWithPermission
                 : NavigationScheme::kFileWithoutPermission;
  }

  base::UmaHistogramEnumeration("Extensions.Navigation.Scheme", scheme);
}

}  // namespace

ExtensionTabUtil::OpenTabParams::OpenTabParams() = default;

ExtensionTabUtil::OpenTabParams::~OpenTabParams() = default;

// Opens a new tab for a given extension. Returns nullptr and sets |error| if an
// error occurs.
base::expected<base::Value::Dict, std::string> ExtensionTabUtil::OpenTab(
    ExtensionFunction* function,
    const OpenTabParams& params,
    bool user_gesture) {
  ChromeExtensionFunctionDetails chrome_details(function);
  Profile* profile = Profile::FromBrowserContext(function->browser_context());
  // windowId defaults to "current" window.
  int window_id = params.window_id.value_or(extension_misc::kCurrentWindowId);

  Browser* browser = nullptr;
  std::string error;
  if (WindowController* controller =
          GetControllerFromWindowID(chrome_details, window_id, &error)) {
    browser = controller->GetBrowser();
  } else {
    // No matching window.
    if (!params.create_browser_if_needed)
      return base::unexpected(error);

    browser = CreateAndShowBrowser(profile, user_gesture, &error);
    if (!browser)
      return base::unexpected(error);
  }

  // Ensure the selected browser is normal.
  if (!browser->is_type_normal() && browser->IsAttemptingToCloseBrowser())
    browser = chrome::FindTabbedBrowser(
        profile, function->include_incognito_information());
  if (!browser || !browser->window()) {
    return base::unexpected(kNoCurrentWindowError);
  }

  // TODO(jstritar): Add a constant, chrome.tabs.TAB_ID_ACTIVE, that
  // represents the active tab.
  WebContents* opener = nullptr;
  Browser* opener_browser = nullptr;
  if (params.opener_tab_id) {
    if (!GetTabById(*params.opener_tab_id, profile,
                    function->include_incognito_information(), &opener_browser,
                    nullptr, &opener, nullptr)) {
      return base::unexpected(ErrorUtils::FormatErrorMessage(
          kTabNotFoundError, base::NumberToString(*params.opener_tab_id)));
    }
  }

  // TODO(rafaelw): handle setting remaining tab properties:
  // -title
  // -favIconUrl

  GURL url(chrome::kChromeUINewTabURL);
  if (params.url) {
    ASSIGN_OR_RETURN(url,
                     PrepareURLForNavigation(*params.url, function->extension(),
                                             function->browser_context()));
  }

  // Default to foreground for the new tab. The presence of 'active' property
  // will override this default.
  bool active = params.active.value_or(true);

  // Default to not pinning the tab. Setting the 'pinned' property to true
  // will override this default.
  bool pinned = params.pinned.value_or(false);

  // We can't load extension URLs into incognito windows unless the extension
  // uses split mode. Special case to fall back to a tabbed window.
  if (url.SchemeIs(kExtensionScheme) &&
      (!function->extension() ||
       !IncognitoInfo::IsSplitMode(function->extension())) &&
      browser->profile()->IsOffTheRecord()) {
    Profile* original_profile = browser->profile()->GetOriginalProfile();

    browser = chrome::FindTabbedBrowser(original_profile, false);
    if (!browser) {
      browser = CreateBrowser(original_profile, user_gesture);
      if (!browser) {
        return base::unexpected(kBrowserWindowNotAllowed);
      }
      browser->window()->Show();
    }
  }

  if (opener_browser && browser != opener_browser) {
    return base::unexpected(
        "Tab opener must be in the same window as the updated tab.");
  }

  // If index is specified, honor the value, but keep it bound to
  // -1 <= index <= tab_strip->count() where -1 invokes the default behavior.
  int index = params.index.value_or(-1);
  index = std::clamp(index, -1, browser->tab_strip_model()->count());

  int add_types = active ? AddTabTypes::ADD_ACTIVE : AddTabTypes::ADD_NONE;
  add_types |= AddTabTypes::ADD_FORCE_INDEX;
  if (pinned)
    add_types |= AddTabTypes::ADD_PINNED;
  NavigateParams navigate_params(browser, url, ui::PAGE_TRANSITION_LINK);
  navigate_params.disposition = active
                                    ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                                    : WindowOpenDisposition::NEW_BACKGROUND_TAB;
  navigate_params.tabstrip_index = index;
  navigate_params.user_gesture = false;
  navigate_params.tabstrip_add_types = add_types;
  base::WeakPtr<content::NavigationHandle> handle = Navigate(&navigate_params);
  if (handle && params.bookmark_id) {
    ChromeNavigationUIData* ui_data =
        static_cast<ChromeNavigationUIData*>(handle->GetNavigationUIData());
    ui_data->set_bookmark_id(*params.bookmark_id);
  }

  // This happens in locked fullscreen mode.
  if (!navigate_params.navigated_or_inserted_contents) {
    return base::unexpected(kLockedFullscreenModeNewTabError);
  }

  // The tab may have been created in a different window, so make sure we look
  // at the right tab strip.
  TabStripModel* tab_strip = navigate_params.browser->tab_strip_model();
  const int new_index = tab_strip->GetIndexOfWebContents(
      navigate_params.navigated_or_inserted_contents);
  if (opener) {
    // Only set the opener if the opener tab is in the same tab strip as the
    // new tab.
    if (tab_strip->GetIndexOfWebContents(opener) != TabStripModel::kNoTab)
      tab_strip->SetOpenerOfWebContentsAt(new_index, opener);
  }

  if (active)
    navigate_params.navigated_or_inserted_contents->SetInitialFocus();

  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(
          function->extension(), function->source_context_type(),
          navigate_params.navigated_or_inserted_contents);

  // Return data about the newly created tab.
  return ExtensionTabUtil::CreateTabObject(
             navigate_params.navigated_or_inserted_contents, scrub_tab_behavior,
             function->extension(), tab_strip, new_index)
      .ToValue();
}

WindowController* ExtensionTabUtil::GetControllerFromWindowID(
    const ChromeExtensionFunctionDetails& details,
    int window_id,
    std::string* error) {
  if (window_id == extension_misc::kCurrentWindowId) {
    if (WindowController* window_controller =
            details.GetCurrentWindowController()) {
      return window_controller;
    }
    if (error) {
      *error = kNoCurrentWindowError;
    }
    return nullptr;
  }
  return GetControllerInProfileWithId(
      Profile::FromBrowserContext(details.function()->browser_context()),
      window_id, details.function()->include_incognito_information(), error);
}

WindowController* ExtensionTabUtil::GetControllerInProfileWithId(
    Profile* profile,
    int window_id,
    bool also_match_incognito_profile,
    std::string* error_message) {
  Profile* incognito_profile =
      also_match_incognito_profile
          ? profile->GetPrimaryOTRProfile(/*create_if_needed=*/false)
          : nullptr;
  for (Browser* browser : *BrowserList::GetInstance()) {
    if ((browser->profile() == profile ||
         browser->profile() == incognito_profile)) {
      WindowController* controller = WindowControllerFromBrowser(browser);
      if (controller->GetWindowId() == window_id) {
        return controller;
      }
    }
  }

  if (error_message) {
    *error_message = ErrorUtils::FormatErrorMessage(
        kWindowNotFoundError, base::NumberToString(window_id));
  }

  return nullptr;
}

int ExtensionTabUtil::GetWindowId(const Browser* browser) {
  return WindowControllerFromBrowser(browser)->GetWindowId();
}

int ExtensionTabUtil::GetWindowIdOfTabStripModel(
    const TabStripModel* tab_strip_model) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->tab_strip_model() == tab_strip_model)
      return GetWindowId(browser);
  }
  return -1;
}

int ExtensionTabUtil::GetTabId(const WebContents* web_contents) {
  return sessions::SessionTabHelper::IdForTab(web_contents).id();
}

int ExtensionTabUtil::GetWindowIdOfTab(const WebContents* web_contents) {
  return sessions::SessionTabHelper::IdForWindowContainingTab(web_contents)
      .id();
}

// static
std::string ExtensionTabUtil::GetBrowserWindowTypeText(const Browser& browser) {
  return WindowControllerFromBrowser(&browser)->GetWindowTypeText();
}

// static
api::tabs::Tab ExtensionTabUtil::CreateTabObject(
    WebContents* contents,
    ScrubTabBehavior scrub_tab_behavior,
    const Extension* extension,
    TabStripModel* tab_strip,
    int tab_index) {
  if (!tab_strip)
    ExtensionTabUtil::GetTabStripModel(contents, &tab_strip, &tab_index);
  api::tabs::Tab tab_object;
  tab_object.id = GetTabIdForExtensions(contents);
  tab_object.index = tab_index;
  tab_object.window_id = GetWindowIdOfTab(contents);
  tab_object.status = GetLoadingStatus(contents);
  tab_object.last_accessed =
      contents->GetLastActiveTime().InMillisecondsFSinceUnixEpoch();
  tab_object.active = tab_strip && tab_index == tab_strip->active_index();
  tab_object.selected = tab_strip && tab_index == tab_strip->active_index();
  tab_object.highlighted = tab_strip && tab_strip->IsTabSelected(tab_index);
  tab_object.pinned = tab_strip && tab_strip->IsTabPinned(tab_index);

  tab_object.group_id = -1;
  if (tab_strip) {
    std::optional<tab_groups::TabGroupId> group =
        tab_strip->GetTabGroupForTab(tab_index);
    if (group.has_value()) {
      tab_object.group_id = GetGroupId(group.value());
    }
  }

  auto* audible_helper = RecentlyAudibleHelper::FromWebContents(contents);
  bool audible = false;
  if (audible_helper) {
    // WebContents in a tab strip have RecentlyAudible helpers. They endow the
    // tab with a notion of audibility that has a timeout for quiet periods. Use
    // that if available.
    audible = audible_helper->WasRecentlyAudible();
  } else {
    // Otherwise use the instantaneous notion of audibility.
    audible = contents->IsCurrentlyAudible();
  }
  tab_object.audible = audible;
  auto* tab_lifecycle_unit_external =
      resource_coordinator::TabLifecycleUnitExternal::FromWebContents(contents);

  // Note that while a discarded tab *must* have an unloaded status, its
  // possible for an unloaded tab to not be discarded (session restored tabs
  // whose loads have been deferred, for example).
  tab_object.discarded =
      tab_lifecycle_unit_external && tab_lifecycle_unit_external->IsDiscarded();
  DCHECK(!tab_object.discarded ||
         tab_object.status == api::tabs::TabStatus::kUnloaded);
  tab_object.auto_discardable =
      !tab_lifecycle_unit_external ||
      tab_lifecycle_unit_external->IsAutoDiscardable();

  tab_object.muted_info = CreateMutedInfo(contents);
  tab_object.incognito = contents->GetBrowserContext()->IsOffTheRecord();
  gfx::Size contents_size = contents->GetContainerBounds().size();
  tab_object.width = contents_size.width();
  tab_object.height = contents_size.height();

  tab_object.url = contents->GetLastCommittedURL().spec();
  NavigationEntry* pending_entry = contents->GetController().GetPendingEntry();
  if (pending_entry) {
    tab_object.pending_url = pending_entry->GetVirtualURL().spec();
  }
  tab_object.title = base::UTF16ToUTF8(contents->GetTitle());
  // TODO(tjudkins) This should probably use the LastCommittedEntry() for
  // consistency.
  NavigationEntry* visible_entry = contents->GetController().GetVisibleEntry();
  if (visible_entry && visible_entry->GetFavicon().valid) {
    tab_object.fav_icon_url = visible_entry->GetFavicon().url.spec();
  }
  if (tab_strip) {
    tabs::TabModel* opener = tab_strip->GetOpenerOfTabAt(tab_index);
    if (opener) {
      CHECK(opener->contents());
      tab_object.opener_tab_id = GetTabIdForExtensions(opener->contents());
    }
  }

  ScrubTabForExtension(extension, contents, &tab_object, scrub_tab_behavior);
  return tab_object;
}

base::Value::List ExtensionTabUtil::CreateTabList(const Browser* browser,
                                                  const Extension* extension,
                                                  mojom::ContextType context) {
  return WindowControllerFromBrowser(browser)->CreateTabList(extension,
                                                             context);
}

// static
base::Value::Dict ExtensionTabUtil::CreateWindowValueForExtension(
    const Browser& browser,
    const Extension* extension,
    WindowController::PopulateTabBehavior populate_tab_behavior,
    mojom::ContextType context) {
  return WindowControllerFromBrowser(&browser)->CreateWindowValueForExtension(
      extension, populate_tab_behavior, context);
}

// static
api::tabs::MutedInfo ExtensionTabUtil::CreateMutedInfo(
    content::WebContents* contents) {
  DCHECK(contents);
  api::tabs::MutedInfo info;
  info.muted = contents->IsAudioMuted();
  switch (GetTabAudioMutedReason(contents)) {
    case TabMutedReason::NONE:
      break;
    case TabMutedReason::AUDIO_INDICATOR:
    case TabMutedReason::CONTENT_SETTING:
    case TabMutedReason::CONTENT_SETTING_CHROME:
      info.reason = api::tabs::MutedInfoReason::kUser;
      break;
    case TabMutedReason::EXTENSION:
      info.reason = api::tabs::MutedInfoReason::kExtension;
      info.extension_id =
          LastMuteMetadata::FromWebContents(contents)->extension_id;
      DCHECK(!info.extension_id->empty());
      break;
  }
  return info;
}

// static
ExtensionTabUtil::ScrubTabBehavior ExtensionTabUtil::GetScrubTabBehavior(
    const Extension* extension,
    mojom::ContextType context,
    content::WebContents* contents) {
  int tab_id = GetTabId(contents);
  ScrubTabBehavior behavior;
  behavior.committed_info = GetScrubTabBehaviorImpl(
      extension, context, contents->GetLastCommittedURL(), tab_id);
  NavigationEntry* entry = contents->GetController().GetPendingEntry();
  GURL pending_url;
  if (entry) {
    pending_url = entry->GetVirtualURL();
  }
  behavior.pending_info =
      GetScrubTabBehaviorImpl(extension, context, pending_url, tab_id);
  return behavior;
}

// static
ExtensionTabUtil::ScrubTabBehavior ExtensionTabUtil::GetScrubTabBehavior(
    const Extension* extension,
    mojom::ContextType context,
    const GURL& url) {
  ScrubTabBehaviorType type =
      GetScrubTabBehaviorImpl(extension, context, url, api::tabs::TAB_ID_NONE);
  return {type, type};
}

// static
void ExtensionTabUtil::ScrubTabForExtension(
    const Extension* extension,
    content::WebContents* contents,
    api::tabs::Tab* tab,
    ScrubTabBehavior scrub_tab_behavior) {
  // Remove sensitive committed tab info if necessary.
  switch (scrub_tab_behavior.committed_info) {
    case kScrubTabFully:
      tab->url.reset();
      tab->title.reset();
      tab->fav_icon_url.reset();
      break;
    case kScrubTabUrlToOrigin:
      tab->url = GURL(*tab->url).DeprecatedGetOriginAsURL().spec();
      break;
    case kDontScrubTab:
      break;
  }

  // Remove sensitive pending tab info if necessary.
  if (tab->pending_url) {
    switch (scrub_tab_behavior.pending_info) {
      case kScrubTabFully:
        tab->pending_url.reset();
        break;
      case kScrubTabUrlToOrigin:
        tab->pending_url =
            GURL(*tab->pending_url).DeprecatedGetOriginAsURL().spec();
        break;
      case kDontScrubTab:
        break;
    }
  }
}

// static
bool ExtensionTabUtil::GetTabStripModel(const WebContents* web_contents,
                                        TabStripModel** tab_strip_model,
                                        int* tab_index) {
  DCHECK(web_contents);
  DCHECK(tab_strip_model);
  DCHECK(tab_index);

  for (Browser* browser : *BrowserList::GetInstance()) {
    TabStripModel* tab_strip = browser->tab_strip_model();
    int index = tab_strip->GetIndexOfWebContents(web_contents);
    if (index != -1) {
      *tab_strip_model = tab_strip;
      *tab_index = index;
      return true;
    }
  }

  return false;
}

content::WebContents* ExtensionTabUtil::GetActiveTab(Browser* browser) {
  return WindowControllerFromBrowser(browser)->GetActiveTab();
}

// static
bool ExtensionTabUtil::GetTabById(int tab_id,
                                  content::BrowserContext* browser_context,
                                  bool include_incognito,
                                  Browser** browser,
                                  TabStripModel** tab_strip,
                                  WebContents** contents,
                                  int* tab_index) {
  if (tab_id == api::tabs::TAB_ID_NONE)
    return false;
  // If `browser_context` is null, then `Profile::FromBrowserContext` below
  // will return nullptr, and the subsequent call to `GetPrimaryOTRProfile`
  // will crash. Since this can happen during shutdown, early-out to avoid
  // crashing.
  if (!browser_context) {
    return false;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
  Profile* incognito_profile =
      include_incognito
          ? (profile ? profile->GetPrimaryOTRProfile(/*create_if_needed=*/false)
                     : nullptr)
          : nullptr;
  for (Browser* target_browser : *BrowserList::GetInstance()) {
    if (target_browser->profile() == profile ||
        target_browser->profile() == incognito_profile) {
      TabStripModel* target_tab_strip = target_browser->tab_strip_model();
      for (int i = 0; i < target_tab_strip->count(); ++i) {
        WebContents* target_contents = target_tab_strip->GetWebContentsAt(i);
        if (sessions::SessionTabHelper::IdForTab(target_contents).id() ==
            tab_id) {
          if (browser)
            *browser = target_browser;
          if (tab_strip)
            *tab_strip = target_tab_strip;
          if (contents)
            *contents = target_contents;
          if (tab_index)
            *tab_index = i;
          return true;
        }
      }
    }
  }

  if (base::FeatureList::IsEnabled(blink::features::kPrerender2InNewTab)) {
    // Prerendering tab is not visible and it cannot be in `TabStripModel`, if
    // the tab id exists as a prerendering tab, and the API will returns
    // `api::tabs::TAB_INDEX_NONE` for `tab_index` and a valid `WebContents`.
    for (auto rph_iterator = content::RenderProcessHost::AllHostsIterator();
         !rph_iterator.IsAtEnd(); rph_iterator.Advance()) {
      content::RenderProcessHost* rph = rph_iterator.GetCurrentValue();

      // Ignore renderers that aren't ready.
      if (!rph->IsInitializedAndNotDead()) {
        continue;
      }
      // Ignore renderers that aren't from a valid profile. This is either the
      // same profile or the incognito profile if `include_incognito` is true.
      Profile* process_profile =
          Profile::FromBrowserContext(rph->GetBrowserContext());
      if (process_profile != profile &&
          !(include_incognito && profile->IsSameOrParent(process_profile))) {
        continue;
      }

      rph->ForEachRenderFrameHost([&contents, &tab_index, &tab_strip,
                                   tab_id](content::RenderFrameHost* rfh) {
        CHECK(rfh);
        WebContents* web_contents = WebContents::FromRenderFrameHost(rfh);
        CHECK(web_contents);
        if (sessions::SessionTabHelper::IdForTab(web_contents).id() != tab_id) {
          return;
        }
        // We only consider prerendered frames in this loop. Otherwise, we could
        // end up returning a tab for a different web contents that shouldn't be
        // exposed to extensions.
        if (!web_contents->IsPrerenderedFrame(rfh->GetFrameTreeNodeId())) {
          return;
        }

        // TODO(crbug.com/40234240): tab_strip and tab_index are tied to
        // a specific window, and related APIs return WINDOW_ID_NONE for
        // prerendering-into-a-new-tab tabs as a tentaive solution. So these
        // values are set to be invalid here.
        if (tab_strip) {
          *tab_strip = nullptr;
        }

        if (tab_index) {
          *tab_index = api::tabs::TAB_INDEX_NONE;
        }

        if (contents) {
          *contents = web_contents;
        }
      });

      if (contents && *contents) {
        return true;
      }
    }
  }

  return false;
}

// static
bool ExtensionTabUtil::GetTabById(int tab_id,
                                  content::BrowserContext* browser_context,
                                  bool include_incognito,
                                  WebContents** contents) {
  return GetTabById(tab_id, browser_context, include_incognito, nullptr,
                    nullptr, contents, nullptr);
}

// static
int ExtensionTabUtil::GetGroupId(const tab_groups::TabGroupId& id) {
  uint32_t hash = base::PersistentHash(id.ToString());
  return std::abs(static_cast<int>(hash));
}

// static
int ExtensionTabUtil::GetWindowIdOfGroup(const tab_groups::TabGroupId& id) {
  Browser* browser = chrome::FindBrowserWithGroup(id, nullptr);
  if (browser) {
    return browser->session_id().id();
  }
  return -1;
}

// static
bool ExtensionTabUtil::GetGroupById(
    int group_id,
    content::BrowserContext* browser_context,
    bool include_incognito,
    Browser** browser,
    tab_groups::TabGroupId* id,
    const tab_groups::TabGroupVisualData** visual_data,
    std::string* error) {
  if (group_id == -1) {
    return false;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
  Profile* incognito_profile =
      include_incognito && profile->HasPrimaryOTRProfile()
          ? profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)
          : nullptr;
  for (Browser* target_browser : *BrowserList::GetInstance()) {
    if (target_browser->profile() == profile ||
        target_browser->profile() == incognito_profile) {
      TabStripModel* target_tab_strip = target_browser->tab_strip_model();
      if (!target_tab_strip->SupportsTabGroups()) {
        continue;
      }
      for (tab_groups::TabGroupId target_group :
           target_tab_strip->group_model()->ListTabGroups()) {
        if (ExtensionTabUtil::GetGroupId(target_group) == group_id) {
          if (browser) {
            *browser = target_browser;
          }
          if (id) {
            *id = target_group;
          }
          if (visual_data) {
            *visual_data = target_tab_strip->group_model()
                               ->GetTabGroup(target_group)
                               ->visual_data();
          }
          return true;
        }
      }
    }
  }

  *error = ErrorUtils::FormatErrorMessage(kGroupNotFoundError,
                                          base::NumberToString(group_id));

  return false;
}

// static
api::tab_groups::TabGroup ExtensionTabUtil::CreateTabGroupObject(
    const tab_groups::TabGroupId& id,
    const tab_groups::TabGroupVisualData& visual_data) {
  api::tab_groups::TabGroup tab_group_object;
  tab_group_object.id = GetGroupId(id);
  tab_group_object.collapsed = visual_data.is_collapsed();
  tab_group_object.color = ColorIdToColor(visual_data.color());
  tab_group_object.title = base::UTF16ToUTF8(visual_data.title());
  tab_group_object.window_id = GetWindowIdOfGroup(id);

  return tab_group_object;
}

// static
std::optional<api::tab_groups::TabGroup> ExtensionTabUtil::CreateTabGroupObject(
    const tab_groups::TabGroupId& id) {
  Browser* browser = chrome::FindBrowserWithGroup(id, nullptr);
  if (!browser) {
    return std::nullopt;
  }

  CHECK(browser->tab_strip_model()->SupportsTabGroups());
  TabGroupModel* group_model = browser->tab_strip_model()->group_model();
  const tab_groups::TabGroupVisualData* visual_data =
      group_model->GetTabGroup(id)->visual_data();

  DCHECK(visual_data);

  return CreateTabGroupObject(id, *visual_data);
}

// static
api::tab_groups::Color ExtensionTabUtil::ColorIdToColor(
    const tab_groups::TabGroupColorId& color_id) {
  switch (color_id) {
    case tab_groups::TabGroupColorId::kGrey:
      return api::tab_groups::Color::kGrey;
    case tab_groups::TabGroupColorId::kBlue:
      return api::tab_groups::Color::kBlue;
    case tab_groups::TabGroupColorId::kRed:
      return api::tab_groups::Color::kRed;
    case tab_groups::TabGroupColorId::kYellow:
      return api::tab_groups::Color::kYellow;
    case tab_groups::TabGroupColorId::kGreen:
      return api::tab_groups::Color::kGreen;
    case tab_groups::TabGroupColorId::kPink:
      return api::tab_groups::Color::kPink;
    case tab_groups::TabGroupColorId::kPurple:
      return api::tab_groups::Color::kPurple;
    case tab_groups::TabGroupColorId::kCyan:
      return api::tab_groups::Color::kCyan;
    case tab_groups::TabGroupColorId::kOrange:
      return api::tab_groups::Color::kOrange;
    case tab_groups::TabGroupColorId::kNumEntries:
      NOTREACHED_IN_MIGRATION() << "kNumEntries is not a support color enum.";
      return api::tab_groups::Color::kGrey;
  }

  NOTREACHED_IN_MIGRATION();
  return api::tab_groups::Color::kCyan;
}

// static
tab_groups::TabGroupColorId ExtensionTabUtil::ColorToColorId(
    api::tab_groups::Color color) {
  switch (color) {
    case api::tab_groups::Color::kGrey:
      return tab_groups::TabGroupColorId::kGrey;
    case api::tab_groups::Color::kBlue:
      return tab_groups::TabGroupColorId::kBlue;
    case api::tab_groups::Color::kRed:
      return tab_groups::TabGroupColorId::kRed;
    case api::tab_groups::Color::kYellow:
      return tab_groups::TabGroupColorId::kYellow;
    case api::tab_groups::Color::kGreen:
      return tab_groups::TabGroupColorId::kGreen;
    case api::tab_groups::Color::kPink:
      return tab_groups::TabGroupColorId::kPink;
    case api::tab_groups::Color::kPurple:
      return tab_groups::TabGroupColorId::kPurple;
    case api::tab_groups::Color::kCyan:
      return tab_groups::TabGroupColorId::kCyan;
    case api::tab_groups::Color::kOrange:
      return tab_groups::TabGroupColorId::kOrange;
    case api::tab_groups::Color::kNone:
      NOTREACHED_IN_MIGRATION();
  }

  NOTREACHED_IN_MIGRATION();
  return tab_groups::TabGroupColorId::kGrey;
}

// static
std::vector<content::WebContents*>
ExtensionTabUtil::GetAllActiveWebContentsForContext(
    content::BrowserContext* browser_context,
    bool include_incognito) {
  std::vector<content::WebContents*> active_contents;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  Profile* incognito_profile =
      include_incognito
          ? profile->GetPrimaryOTRProfile(/*create_if_needed=*/false)
          : nullptr;
  for (Browser* target_browser : *BrowserList::GetInstance()) {
    if (target_browser->profile() == profile ||
        target_browser->profile() == incognito_profile) {
      TabStripModel* target_tab_strip = target_browser->tab_strip_model();

      active_contents.push_back(target_tab_strip->GetActiveWebContents());
    }
  }

  return active_contents;
}

// static
bool ExtensionTabUtil::IsWebContentsInContext(
    content::WebContents* web_contents,
    content::BrowserContext* browser_context,
    bool include_incognito) {
  // Look at the WebContents BrowserContext and see if it is the same.
  content::BrowserContext* web_contents_browser_context =
      web_contents->GetBrowserContext();
  if (web_contents_browser_context == browser_context)
    return true;

  // If not it might be to include the incongito mode, so we if the profiles
  // are the same or the parent.
  return include_incognito && Profile::FromBrowserContext(browser_context)
                                  ->IsSameOrParent(Profile::FromBrowserContext(
                                      web_contents_browser_context));
}

GURL ExtensionTabUtil::ResolvePossiblyRelativeURL(const std::string& url_string,
                                                  const Extension* extension) {
  GURL url = GURL(url_string);
  if (!url.is_valid() && extension)
    url = extension->GetResourceURL(url_string);

  return url;
}

bool ExtensionTabUtil::IsKillURL(const GURL& url) {
#if DCHECK_IS_ON()
  // Caller should ensure that |url| is already "fixed up" by
  // url_formatter::FixupURL, which (among many other things) takes care
  // of rewriting about:kill into chrome://kill/.
  if (url.SchemeIs(url::kAboutScheme))
    DCHECK(url.IsAboutBlank() || url.IsAboutSrcdoc());
#endif

  // Disallow common renderer debug URLs.
  // Note: this would also disallow JavaScript URLs, but we already explicitly
  // check for those before calling into here from PrepareURLForNavigation.
  if (blink::IsRendererDebugURL(url)) {
    return true;
  }

  if (!url.SchemeIs(content::kChromeUIScheme)) {
    return false;
  }

  // Also disallow a few more hosts which are not covered by the check above.
  constexpr auto kKillHosts = base::MakeFixedFlatSet<std::string_view>({
      chrome::kChromeUIDelayedHangUIHost,
      chrome::kChromeUIHangUIHost,
      chrome::kChromeUIQuitHost,
      chrome::kChromeUIRestartHost,
      content::kChromeUIBrowserCrashHost,
      content::kChromeUIMemoryExhaustHost,
  });

  return kKillHosts.contains(url.host_piece());
}

base::expected<GURL, std::string> ExtensionTabUtil::PrepareURLForNavigation(
    const std::string& url_string,
    const Extension* extension,
    content::BrowserContext* browser_context) {
  GURL url =
      ExtensionTabUtil::ResolvePossiblyRelativeURL(url_string, extension);

  // Ideally, the URL would only be "fixed" for user input (e.g. for URLs
  // entered into the Omnibox), but some extensions rely on the legacy behavior
  // where all navigations were subject to the "fixing".  See also
  // https://crbug.com/1145381.
  url = url_formatter::FixupURL(url.spec(), "" /* = desired_tld */);

  // Reject invalid URLs.
  if (!url.is_valid()) {
    return base::unexpected(
        ErrorUtils::FormatErrorMessage(kInvalidUrlError, url_string));
  }

  // Don't let the extension use JavaScript URLs in API triggered navigations.
  if (url.SchemeIs(url::kJavaScriptScheme)) {
    return base::unexpected(kJavaScriptUrlsNotAllowedInExtensionNavigations);
  }

  // Don't let the extension crash the browser or renderers.
  if (ExtensionTabUtil::IsKillURL(url)) {
    return base::unexpected(kNoCrashBrowserError);
  }

  // Don't let the extension navigate directly to devtools scheme pages, unless
  // they have applicable permissions.
  if (url.SchemeIs(content::kChromeDevToolsScheme)) {
    bool has_permission =
        extension && (extension->permissions_data()->HasAPIPermission(
                          APIPermissionID::kDevtools) ||
                      extension->permissions_data()->HasAPIPermission(
                          APIPermissionID::kDebugger));
    if (!has_permission) {
      return base::unexpected(kCannotNavigateToDevtools);
    }
  }

  // Don't let the extension navigate directly to chrome-untrusted scheme pages.
  if (url.SchemeIs(content::kChromeUIUntrustedScheme)) {
    return base::unexpected(kCannotNavigateToChromeUntrusted);
  }

  // Don't let the extension navigate directly to file scheme pages, unless
  // they have file access. `extension` can be null if the call is made from
  // non-extension contexts (e.g. WebUI pages). In that case, we allow the
  // navigation as such contexts are trusted and do not have a concept of file
  // access.
  if (extension && IsFileUrl(url) &&
      // PDF viewer extension can navigate to file URLs.
      extension->id() != extension_misc::kPdfExtensionId &&
      !util::AllowFileAccess(extension->id(), browser_context) &&
      !extensions::ExtensionManagementFactory::GetForBrowserContext(
           browser_context)
           ->IsFileUrlNavigationAllowed(extension->id())) {
    return base::unexpected(kFileUrlsNotAllowedInExtensionNavigations);
  }

  if (extension && browser_context) {
    RecordNavigationScheme(url, *extension, browser_context);
  }

  return url;
}

void ExtensionTabUtil::CreateTab(
    std::unique_ptr<WebContents> web_contents,
    const std::string& extension_id,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  Browser* browser = chrome::FindTabbedBrowser(profile, false);
  const bool browser_created = !browser;
  if (!browser)
    browser = CreateBrowser(profile, user_gesture);
  if (!browser)
    return;

  NavigateParams params(browser, std::move(web_contents));

  // The extension_app_id parameter ends up as app_name in the Browser
  // which causes the Browser to return true for is_app().  This affects
  // among other things, whether the location bar gets displayed.
  // TODO(mpcomplete): This seems wrong. What if the extension content is hosted
  // in a tab?
  if (disposition == WindowOpenDisposition::NEW_POPUP)
    params.app_id = extension_id;

  params.disposition = disposition;
  params.window_features = window_features;
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.user_gesture = user_gesture;
  Navigate(&params);

  // Close the browser if Navigate created a new one.
  if (browser_created && (browser != params.browser))
    browser->window()->Close();
}

// static
void ExtensionTabUtil::ForEachTab(
    base::RepeatingCallback<void(WebContents*)> callback) {
  for (auto* web_contents : AllTabContentses())
    callback.Run(web_contents);
}

// static
WindowController* ExtensionTabUtil::GetWindowControllerOfTab(
    const WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (browser != nullptr)
    return browser->extension_window_controller();

  return nullptr;
}

bool ExtensionTabUtil::OpenOptionsPageFromAPI(
    const Extension* extension,
    content::BrowserContext* browser_context) {
  if (!OptionsPageInfo::HasOptionsPage(extension))
    return false;
  Profile* profile = Profile::FromBrowserContext(browser_context);
  // This version of OpenOptionsPage() is only called when the extension
  // initiated the command via chrome.runtime.openOptionsPage. For a spanning
  // mode extension, this API could only be called from a regular profile, since
  // that's the only place it's running.
  DCHECK(!profile->IsOffTheRecord() || IncognitoInfo::IsSplitMode(extension));
  Browser* browser = chrome::FindBrowserWithProfile(profile);
  if (!browser)
    browser = CreateBrowser(profile, true);
  if (!browser)
    return false;
  return extensions::ExtensionTabUtil::OpenOptionsPage(extension, browser);
}

bool ExtensionTabUtil::OpenOptionsPage(const Extension* extension,
                                       Browser* browser) {
  return WindowControllerFromBrowser(browser)->OpenOptionsPage(extension);
}

// static
bool ExtensionTabUtil::BrowserSupportsTabs(Browser* browser) {
  return browser && !browser->is_type_devtools();
}

// static
api::tabs::TabStatus ExtensionTabUtil::GetLoadingStatus(WebContents* contents) {
  if (contents->IsLoading()) {
    return api::tabs::TabStatus::kLoading;
  }

  // Anything that isn't backed by a process is considered unloaded. Discarded
  // tabs should also be considered unloaded as the tab itself may be retained
  // but the hosted document discarded to reclaim resources.
  if (!HasValidMainFrameProcess(contents) || contents->WasDiscarded()) {
    return api::tabs::TabStatus::kUnloaded;
  }

  // Otherwise its considered loaded.
  return api::tabs::TabStatus::kComplete;
}

void ExtensionTabUtil::ClearBackForwardCache() {
  ForEachTab(base::BindRepeating([](WebContents* web_contents) {
    web_contents->GetController().GetBackForwardCache().Flush();
  }));
}

// static
bool ExtensionTabUtil::IsTabStripEditable() {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser && !browser->window()->IsTabStripEditable())
      return false;
  }
  return true;
}

// static
TabStripModel* ExtensionTabUtil::GetEditableTabStripModel(Browser* browser) {
  if (!IsTabStripEditable())
    return nullptr;
  return browser->tab_strip_model();
}

// static
bool ExtensionTabUtil::TabIsInSavedTabGroup(content::WebContents* contents,
                                            TabStripModel* tab_strip_model) {
  // If the tab_strip_model is empty, find the contents in one of the browsers.
  if (!tab_strip_model) {
    CHECK(contents);
    Browser* browser = chrome::FindBrowserWithTab(contents);

    // if the webcontents isn't in any tabstrip, its not in a saved tab group.
    if (!browser) {
      return false;
    }
    tab_strip_model = browser->tab_strip_model();
  }

  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(
          tab_strip_model->profile());

  // If the service failed to start, then there are no saved tab groups.
  if (!tab_group_service) {
    return false;
  }

  // If the tab is not in a group, then its not going to be in a saved group.
  int index = tab_strip_model->GetIndexOfWebContents(contents);
  std::optional<tab_groups::TabGroupId> tab_group_id =
      tab_strip_model->GetTabGroupForTab(index);
  if (!tab_group_id.has_value()) {
    return false;
  }

  return tab_group_service->GetGroup(tab_group_id.value()).has_value();
}

}  // namespace extensions
