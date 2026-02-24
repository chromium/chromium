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

#include "base/auto_reset.h"
#include "base/containers/fixed_flat_set.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/browser_window_util.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_muted_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tab_groups/tab_group_id.h"  // nogncheck
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_util.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/ui/browser.h"                             // nogncheck
#include "chrome/browser/ui/browser_finder.h"                      // nogncheck
#include "chrome/browser/ui/browser_window.h"                      // nogncheck
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/recently_audible_helper.h"             // nogncheck
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"  // nogncheck
#include "chrome/browser/ui/tabs/tab_enums.h"                      // nogncheck
#include "chrome/browser/ui/tabs/tab_group_model.h"                // nogncheck
#include "chrome/browser/ui/tabs/tab_strip_model.h"                // nogncheck
#include "chrome/browser/ui/tabs/tab_utils.h"                      // nogncheck
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/back_forward_cache.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using content::NavigationEntry;
using content::WebContents;
using extensions::mojom::APIPermissionID;

namespace extensions {

namespace {

// Whether to disable tab list editing for testing purposes.
bool g_disable_tab_list_editing_for_testing = false;

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

// Guaranteed non-null for any initialized browser window when the extensions
// system is still attached to the Browser (callers shouldn't need to null
// check).
WindowController* WindowControllerFromBrowser(BrowserWindowInterface* browser) {
  return BrowserExtensionWindowController::From(browser);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)

BrowserWindowInterface* CreateBrowser(Profile* profile, bool user_gesture) {
  if (Browser::GetCreationStatusForProfile(profile) !=
      Browser::CreationStatus::kOk) {
    return nullptr;
  }

  BrowserWindowCreateParams params(BrowserWindowInterface::TYPE_NORMAL,
                                   *profile, user_gesture);
  // TODO(https://crbug.com/430344931): When this is ported to android
  // platforms, this window isn't guaranteed to be fully initialized.
  return CreateBrowserWindow(std::move(params));
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// Use this function for reporting a tab id to an extension. It will
// take care of setting the id to TAB_ID_NONE if necessary (for
// example with devtools).
int GetTabIdForExtensions(WebContents& web_contents) {
  BrowserWindowInterface* browser =
      browser_window_util::GetBrowserForTabContents(web_contents);
  if (browser && !ExtensionTabUtil::BrowserSupportsTabs(browser)) {
    return -1;
  }
  return sessions::SessionTabHelper::IdForTab(&web_contents).id();
}

bool IsFileUrl(const GURL& url) {
  return url.SchemeIsFile() || (url.SchemeIs(content::kViewSourceScheme) &&
                                GURL(url.GetContentPiece()).SchemeIsFile());
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

bool ShouldOpenInTab(const Extension* extension) {
// We always open the options page in new tab on android. Embedding the page on
// chrome://extensions is done with guest_view, but it's not enabled on android.
#if BUILDFLAG(IS_ANDROID)
  return true;
#else
  return OptionsPageInfo::ShouldOpenInTab(extension);
#endif
}

// Returns the URL to the extension's options page, if any.
std::optional<GURL> GetOptionsPageUrlToNavigate(const Extension* extension) {
  if (!OptionsPageInfo::HasOptionsPage(extension)) {
    return std::nullopt;
  }

  if (ShouldOpenInTab(extension)) {
    // Options page tab is simply e.g. chrome-extension://.../options.html.
    return OptionsPageInfo::GetOptionsPage(extension);
  } else {
    // Options page tab is Extension settings pointed at that Extension's ID,
    // e.g. chrome://extensions?options=...
    GURL::Replacements replacements;
    const std::string query = base::StringPrintf("options=%s", extension->id());
    replacements.SetQueryStr(query);
    return GURL(chrome::kChromeUIExtensionsURL).ReplaceComponents(replacements);
  }
}

// Returns the browser that contains the tab group with `id` or null if none is
// found.
BrowserWindowInterface* FindBrowserWithGroup(const tab_groups::TabGroupId& id) {
  for (BrowserWindowInterface* const bwi : GetAllBrowserWindowInterfaces()) {
    TabListInterface* const tab_list = TabListInterface::From(bwi);
    if (tab_list && tab_list->ContainsTabGroup(id)) {
      return bwi;
    }
  }
  return nullptr;
}

// Gets the window ID that the group belongs to.
int GetWindowIdOfGroup(const tab_groups::TabGroupId& id) {
  if (BrowserWindowInterface* const browser = FindBrowserWithGroup(id);
      browser) {
    return browser->GetSessionID().id();
  }
  return -1;
}

// Creates a tab MutedInfo object (see chrome/common/extensions/api/tabs.json)
// with information about the mute state of a browser tab.
api::tabs::MutedInfo CreateMutedInfo(content::WebContents* contents) {
  DCHECK(contents);
  api::tabs::MutedInfo info;
  info.muted = contents->IsAudioMuted();
  switch (GetTabAudioMutedReason(contents)) {
    case TabMutedReason::kNone:
      break;
    case TabMutedReason::kAudioIndicator:
    case TabMutedReason::kContentSetting:
    case TabMutedReason::kContentSettingChrome:
      info.reason = api::tabs::MutedInfoReason::kUser;
      break;
    case TabMutedReason::kExtension:
      info.reason = api::tabs::MutedInfoReason::kExtension;
      info.extension_id =
          LastMuteMetadata::FromWebContents(contents)->extension_id;
      DCHECK(!info.extension_id->empty());
      break;
  }
  return info;
}

}  // namespace

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
  const Profile* incognito_profile =
      also_match_incognito_profile
          ? profile->GetPrimaryOTRProfile(/*create_if_needed=*/false)
          : nullptr;
  for (WindowController* window_controller :
       *WindowControllerList::GetInstance()) {
    const Profile* controller_profile = window_controller->profile();
    if ((controller_profile == profile ||
         controller_profile == incognito_profile) &&
        window_controller->GetWindowId() == window_id) {
      return window_controller;
    }
  }

  if (error_message) {
    *error_message = ErrorUtils::FormatErrorMessage(
        kWindowNotFoundError, base::NumberToString(window_id));
  }

  return nullptr;
}

int ExtensionTabUtil::GetWindowId(BrowserWindowInterface* browser) {
  return WindowControllerFromBrowser(browser)->GetWindowId();
}

int ExtensionTabUtil::GetTabId(const WebContents* web_contents) {
  return sessions::SessionTabHelper::IdForTab(web_contents).id();
}

int ExtensionTabUtil::GetWindowIdOfTab(const WebContents* web_contents) {
  return sessions::SessionTabHelper::IdForWindowContainingTab(web_contents)
      .id();
}

// static
api::tabs::Tab ExtensionTabUtil::CreateTabObject(
    WebContents* contents,
    ScrubTabBehavior scrub_tab_behavior,
    const Extension* extension,
    TabListInterface* tab_list,
    int tab_index) {
  if (!tab_list) {
    GetTabListInterface(*contents, &tab_list, &tab_index);
  }
  api::tabs::Tab tab_object;
  tab_object.id = GetTabIdForExtensions(*contents);
  tab_object.index = tab_index;
  tab_object.window_id = GetWindowIdOfTab(contents);
  tab_object.status = GetLoadingStatus(contents);
  tab_object.last_accessed =
      contents->GetLastActiveTime().InMillisecondsFSinceUnixEpoch();

  tabs::TabInterface* tab_interface =
      tab_list ? tab_list->GetTab(tab_index) : nullptr;

  bool is_active = tab_interface && tab_interface->IsActivated();
  tab_object.active = is_active;
  tab_object.selected = is_active;
  tab_object.highlighted = tab_interface && tab_interface->IsSelected();
  tab_object.pinned = tab_interface && tab_interface->IsPinned();

  tab_object.group_id = -1;
  if (tab_interface) {
    std::optional<tab_groups::TabGroupId> group = tab_interface->GetGroup();
    if (group.has_value()) {
      tab_object.group_id = GetGroupId(group.value());
    }
  }

  tab_object.split_view_id = -1;
  if (tab_interface) {
    std::optional<split_tabs::SplitTabId> split = tab_interface->GetSplit();
    if (split.has_value()) {
      tab_object.split_view_id = GetSplitId(split.value());
    }
  }

  auto get_audible = [contents]() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    auto* audible_helper = RecentlyAudibleHelper::FromWebContents(contents);
    if (audible_helper) {
      // WebContents in a tab strip have RecentlyAudible helpers. They endow
      // the tab with a notion of audibility that has a timeout for quiet
      // periods. Use that if available.
      return audible_helper->WasRecentlyAudible();
    }
#endif
    // Otherwise use the instantaneous notion of audibility.
    return contents->IsCurrentlyAudible();
  };

  tab_object.audible = get_audible();

#if BUILDFLAG(IS_ANDROID)
  tab_object.discarded = contents->WasDiscarded();
  // TODO(crbug.com/371432155): Determine auto-discardable and frozen states on
  // desktop Android where the TabLifecycleUnit is not available.
#else
  auto* tab_lifecycle_unit_external =
      resource_coordinator::TabLifecycleUnitExternal::FromWebContents(contents);

  // Note that while a discarded tab *must* have an unloaded status, its
  // possible for an unloaded tab to not be discarded (session restored tabs
  // whose loads have been deferred, for example).
  tab_object.discarded = tab_lifecycle_unit_external &&
                         tab_lifecycle_unit_external->GetTabState() ==
                             ::mojom::LifecycleUnitState::DISCARDED;
  DCHECK(!tab_object.discarded ||
         tab_object.status == api::tabs::TabStatus::kUnloaded);
  tab_object.auto_discardable =
      !tab_lifecycle_unit_external ||
      tab_lifecycle_unit_external->IsAutoDiscardable();
  tab_object.frozen = tab_lifecycle_unit_external &&
                      tab_lifecycle_unit_external->GetTabState() ==
                          ::mojom::LifecycleUnitState::FROZEN;
#endif  // BUILDFLAG(IS_ANDROID)

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

#if BUILDFLAG(ENABLE_EXTENSIONS)
  TabStripModel* tab_strip = nullptr;
  GetTabStripModel(contents, &tab_strip, &tab_index);
  if (tab_strip) {
    tabs::TabInterface* opener = tab_strip->GetOpenerOfTabAt(tab_index);
    if (opener) {
      content::WebContents* opener_contents = opener->GetContents();
      CHECK(opener_contents);
      tab_object.opener_tab_id = GetTabIdForExtensions(*opener_contents);
    }
  }
#endif

  ScrubTabForExtension(extension, contents, &tab_object, scrub_tab_behavior);
  return tab_object;
}

// static
base::ListValue ExtensionTabUtil::CreateTabList(BrowserWindowInterface* browser,
                                                const Extension* extension,
                                                mojom::ContextType context) {
  return WindowControllerFromBrowser(browser)->CreateTabList(extension,
                                                             context);
}

// static
base::DictValue ExtensionTabUtil::CreateWindowValueForExtension(
    BrowserWindowInterface& browser,
    const Extension* extension,
    WindowController::PopulateTabBehavior populate_tab_behavior,
    mojom::ContextType context) {
  return WindowControllerFromBrowser(&browser)->CreateWindowValueForExtension(
      extension, populate_tab_behavior, context);
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

bool ExtensionTabUtil::GetTabListInterface(content::WebContents& web_contents,
                                           TabListInterface** tab_list_out,
                                           int* index_out) {
  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(&web_contents);
  if (!tab_interface) {
    return false;
  }

  BrowserWindowInterface* browser =
#if BUILDFLAG(IS_ANDROID)
      browser_window_util::GetBrowserForTabContents(web_contents);
#else
      tab_interface->GetBrowserWindowInterface();
#endif

  if (!browser) {
    return false;
  }

  TabListInterface* tab_list = TabListInterface::From(browser);
  if (!tab_list) {
    return false;
  }

  // Find the index of the tab within the browser window.
  // TODO(https://crbug.com/415961057): This is clunky. Let's add a
  // GetIndexOfTab() method.
  std::vector<tabs::TabInterface*> all_tabs = tab_list->GetAllTabs();
  int index = -1;
  for (size_t i = 0; i < all_tabs.size(); ++i) {
    if (all_tabs[i] == tab_interface) {
      index = i;
      break;
    }
  }

  // Even though we got here by looking at the tab strip from the browser window
  // we got from the tab, it's possible the tab isn't in the tab strip. One case
  // in which this happens is if the tab is in the process of being removed.
  if (index == -1) {
    return false;
  }

  *index_out = index;
  *tab_list_out = tab_list;
  return true;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// static
bool ExtensionTabUtil::GetTabStripModel(const WebContents* web_contents,
                                        TabStripModel** tab_strip_model,
                                        int* tab_index) {
  DCHECK(web_contents);
  DCHECK(tab_strip_model);
  DCHECK(tab_index);

  bool found = false;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [web_contents, tab_strip_model, tab_index,
       &found](BrowserWindowInterface* browser_window_interface) {
        TabStripModel* tab_strip = browser_window_interface->GetTabStripModel();
        int index = tab_strip->GetIndexOfWebContents(web_contents);
        if (index != -1) {
          *tab_strip_model = tab_strip;
          *tab_index = index;
          found = true;
          return false;
        }
        return true;
      });

  return found;
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// static
bool ExtensionTabUtil::GetTabById(int tab_id,
                                  content::BrowserContext* browser_context,
                                  bool include_incognito,
                                  WindowController** out_window,
                                  WebContents** out_contents,
                                  int* out_tab_index) {
  // Zero the output parameters so they have predictable values on failure.
  if (out_window) {
    *out_window = nullptr;
  }
  if (out_contents) {
    *out_contents = nullptr;
  }
  if (out_tab_index) {
    *out_tab_index = api::tabs::TAB_INDEX_NONE;
  }

  if (tab_id == api::tabs::TAB_ID_NONE)
    return false;
  // `browser_context` can be null during shutdown.
  if (!browser_context) {
    return false;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
  Profile* incognito_profile =
      include_incognito
          ? (profile ? profile->GetPrimaryOTRProfile(/*create_if_needed=*/false)
                     : nullptr)
          : nullptr;

  for (WindowController* window : *WindowControllerList::GetInstance()) {
    if (window->profile() != profile &&
        window->profile() != incognito_profile) {
      continue;
    }
    for (int i = 0; i < window->GetTabCount(); ++i) {
      WebContents* target_contents = window->GetWebContentsAt(i);
      if (sessions::SessionTabHelper::IdForTab(target_contents).id() ==
          tab_id) {
        if (out_window) {
          *out_window = window;
        }
        if (out_contents) {
          *out_contents = target_contents;
        }
        if (out_tab_index) {
          *out_tab_index = i;
        }
        return true;
      }
    }
  }
#if BUILDFLAG(IS_ANDROID)
  // Some Android test code can create a tab model without a corresponding
  // browser window (e.g. ExtensionBrowserTest::PlatformOpenURLOffTheRecord) so
  // search those tab models as well.
  // TODO(crbug.com/424860292): Delete this code when CreateBrowserWindow()
  // works on desktop Android, including for incognito windows.
  for (const TabModel* const tab_model : TabModelList::models()) {
    if (tab_model->GetProfile() != profile &&
        tab_model->GetProfile() != incognito_profile) {
      continue;
    }
    for (int i = 0; i < tab_model->GetTabCount(); ++i) {
      WebContents* contents = tab_model->GetWebContentsAt(i);
      if (!contents) {
        continue;
      }
      if (sessions::SessionTabHelper::IdForTab(contents).id() != tab_id) {
        continue;
      }
      if (out_contents) {
        *out_contents = contents;
      }
      if (out_tab_index) {
        *out_tab_index = i;
      }
      return true;
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)
  // Prerendering tab is not visible and it cannot be in `TabStripModel`, if the
  // tab id exists as a prerendering tab, and the API will returns
  // `api::tabs::TAB_INDEX_NONE` for `out_tab_index` and a valid `WebContents`.
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

    content::WebContents* found_prerender_contents = nullptr;
    rph->ForEachRenderFrameHost([&found_prerender_contents,
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

      found_prerender_contents = web_contents;
    });

    if (found_prerender_contents && out_contents) {
      *out_contents = found_prerender_contents;
      return true;
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
                    contents, nullptr);
}

// static
int ExtensionTabUtil::GetGroupId(const tab_groups::TabGroupId& id) {
  uint32_t hash = base::PersistentHash(id.ToString());
  return std::abs(static_cast<int>(hash));
}

// static
int ExtensionTabUtil::GetSplitId(const split_tabs::SplitTabId& id) {
  uint32_t hash = base::PersistentHash(id.ToString());
  return std::abs(static_cast<int>(hash));
}

// static
bool ExtensionTabUtil::SupportsTabGroups(BrowserWindowInterface* browser) {
  CHECK(browser);
#if BUILDFLAG(IS_ANDROID)
  // Android only supports tab groups for normal browser windows.
  return browser->GetType() == BrowserWindowInterface::TYPE_NORMAL;
#else
  // Other platforms have more complex logic (i.e. more browser types).
  return browser->GetTabStripModel()->SupportsTabGroups();
#endif
}

// static
bool ExtensionTabUtil::GetGroupById(
    int group_id,
    content::BrowserContext* browser_context,
    bool include_incognito,
    WindowController** out_window,
    tab_groups::TabGroupId* out_id,
    tab_groups::TabGroupVisualData* out_visual_data,
    std::string* error) {
  // Zero output parameters for the error cases.
  if (out_window) {
    *out_window = nullptr;
  }
  if (out_visual_data) {
    *out_visual_data = {};
  }

  if (group_id == -1) {
    return false;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
  Profile* incognito_profile =
      include_incognito && profile->HasPrimaryOTRProfile()
          ? profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)
          : nullptr;
  for (WindowController* target_window : *WindowControllerList::GetInstance()) {
    if (target_window->profile() != profile &&
        target_window->profile() != incognito_profile) {
      continue;
    }
    BrowserWindowInterface* target_browser =
        target_window->GetBrowserWindowInterface();
    if (!target_browser) {
      continue;
    }
    if (!SupportsTabGroups(target_browser)) {
      continue;
    }
    TabListInterface* tab_list = TabListInterface::From(target_browser);
    if (!tab_list) {
      continue;
    }
    for (tab_groups::TabGroupId target_group : tab_list->ListTabGroups()) {
      if (ExtensionTabUtil::GetGroupId(target_group) == group_id) {
        if (out_window) {
          *out_window = target_window;
        }
        if (out_id) {
          *out_id = target_group;
        }
        if (out_visual_data) {
          std::optional<tab_groups::TabGroupVisualData> visual_data =
              tab_list->GetTabGroupVisualData(target_group);
          if (visual_data.has_value()) {
            *out_visual_data = visual_data.value();
          }
        }
        return true;
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

  tab_group_object.shared = GetSharedStateOfGroup(id);
  return tab_group_object;
}

// static
bool ExtensionTabUtil::GetSharedStateOfGroup(const tab_groups::TabGroupId& id) {
  if (!data_sharing::features::IsDataSharingFunctionalityEnabled()) {
    return false;
  }

  BrowserWindowInterface* browser = FindBrowserWithGroup(id);
  if (!browser) {
    return false;
  }

  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          browser->GetProfile());
  if (!tab_group_service) {
    return false;
  }

#if BUILDFLAG(IS_ANDROID)
  // TabGroupService uses a different type on Android.
  const base::Token local_id = id.token();
#else
  const tab_groups::TabGroupId local_id = id;
#endif
  std::optional<tab_groups::SavedTabGroup> saved_group =
      tab_group_service->GetGroup(local_id);
  if (!saved_group) {
    return false;
  }

  return saved_group->is_shared_tab_group();
}

// static
std::optional<api::tab_groups::TabGroup> ExtensionTabUtil::CreateTabGroupObject(
    const tab_groups::TabGroupId& id) {
  BrowserWindowInterface* browser = FindBrowserWithGroup(id);
  if (!browser) {
    return std::nullopt;
  }
  CHECK(SupportsTabGroups(browser));
  TabListInterface* tab_list = TabListInterface::From(browser);
  if (!tab_list) {
    return std::nullopt;
  }
  std::optional<tab_groups::TabGroupVisualData> visual_data =
      tab_list->GetTabGroupVisualData(id);
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
      NOTREACHED() << "kNumEntries is not a support color enum.";
  }

  NOTREACHED();
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
      NOTREACHED();
  }

  NOTREACHED();
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
#if BUILDFLAG(IS_ANDROID)
  for (TabModel* tab_model : TabModelList::models()) {
    if (tab_model->GetProfile() == profile ||
        tab_model->GetProfile() == incognito_profile) {
      // On Android, not every tab has a WebContents, so check for null.
      auto* web_contents = tab_model->GetActiveWebContents();
      if (web_contents) {
        active_contents.push_back(web_contents);
      }
    }
  }
#else
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [profile, incognito_profile,
       &active_contents](BrowserWindowInterface* browser_window_interface) {
        const Profile* browser_profile = browser_window_interface->GetProfile();
        if (browser_profile == profile ||
            browser_profile == incognito_profile) {
          active_contents.push_back(browser_window_interface->GetTabStripModel()
                                        ->GetActiveWebContents());
        }
        return true;
      });
#endif  // BUILDFLAG(IS_ANDROID)

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

  // If not it might be to include the incognito mode, so we if the profiles
  // are the same or the parent.
  return include_incognito && Profile::FromBrowserContext(browser_context)
                                  ->IsSameOrParent(Profile::FromBrowserContext(
                                      web_contents_browser_context));
}

GURL ExtensionTabUtil::ResolvePossiblyRelativeURL(const std::string& url_string,
                                                  const Extension* extension) {
  GURL url = GURL(url_string);
  if (!url.is_valid() && extension) {
    url = extension->ResolveExtensionURL(url_string);
  }

  return url;
}

void ExtensionTabUtil::NavigateToURL(WindowOpenDisposition disposition,
                                     content::WebContents* web_contents,
                                     const GURL& url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  NavigateParams params(chrome::FindBrowserWithTab(web_contents), url,
                        ui::PAGE_TRANSITION_FROM_API);
  params.disposition = disposition;
  params.window_action = NavigateParams::WindowAction::kShowWindow;
  if (web_contents) {
    params.source_contents = web_contents;
  }
  Navigate(&params);
#else
  // Fow now, only current tab and new foreground tab disposition are supported
  // on Android.
  // TODO(crbug.com//440173000): Support other window dispositions for Android.
  CHECK(disposition == WindowOpenDisposition::CURRENT_TAB ||
        disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB);
  content::OpenURLParams params(url, content::Referrer(), disposition,
                                ui::PAGE_TRANSITION_FROM_API,
                                /*is_renderer_initiated=*/false);
  web_contents->OpenURL(params,
                        /*navigation_handle_callback=*/{});
#endif
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

  return kKillHosts.contains(url.host());
}

base::expected<GURL, std::string> ExtensionTabUtil::PrepareURLForNavigation(
    const std::string& url_string,
    const Extension* extension,
    content::BrowserContext* browser_context) {
  GURL url =
      ExtensionTabUtil::ResolvePossiblyRelativeURL(url_string, extension);
  // TODO(crbug.com/385086924): url_formatter::FixupURL transforms a URL
  // with a 'mailto' scheme into a URL with an HTTP scheme. This is a
  // mitigation pending a fix for the bug.
  if (url.SchemeIs(url::kMailToScheme)) {
    return url;
  }

  // Ideally, the URL would only be "fixed" for user input (e.g. for URLs
  // entered into the Omnibox), but some extensions rely on the legacy behavior
  // where all navigations were subject to the "fixing".  See also
  // https://crbug.com/1145381.
  url = url_formatter::FixupURL(url.spec());

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

  // Don't let the extension navigate directly to devtools scheme pages.
  if (url.SchemeIs(content::kChromeDevToolsScheme)) {
    return base::unexpected(kCannotNavigateToDevtools);
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

#if BUILDFLAG(ENABLE_EXTENSIONS)
void ExtensionTabUtil::CreateTab(
    std::unique_ptr<WebContents> web_contents,
    const std::string& extension_id,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  CHECK(profile);
  BrowserWindowInterface* browser =
      browser_window_util::GetLastActiveNormalBrowserWithProfile(
          *profile, /*include_incognito_or_parent=*/false);
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
  params.window_action = NavigateParams::WindowAction::kShowWindow;
  params.user_gesture = user_gesture;
  Navigate(&params);

  // Close the browser if Navigate created a new one.
  if (browser_created && (browser != params.browser)) {
    browser->GetWindow()->Close();
  }
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// static
void ExtensionTabUtil::ForEachTab(
    base::RepeatingCallback<void(WebContents*)> callback) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  tabs::ForEachTabInterface([&callback](tabs::TabInterface* tab) {
    callback.Run(tab->GetContents());
    return true;
  });
#else
  // Android has its own notion of the tab strip and cannot use the code above.
  for (TabModel* tab_model : TabModelList::models()) {
    int tab_count = tab_model->GetTabCount();
    for (int i = 0; i < tab_count; ++i) {
      auto* web_contents = tab_model->GetWebContentsAt(i);
      // On Android, not every tab is guaranteed to have a WebContents.
      if (web_contents) {
        callback.Run(web_contents);
      }
    }
  }
#endif
}

// static
bool ExtensionTabUtil::OpenOptionsPageFromWebContents(
    const Extension* extension,
    content::WebContents* web_contents) {
  const std::optional<GURL> url = GetOptionsPageUrlToNavigate(extension);
  if (!url) {
    return false;
  }
  const bool open_in_tab = ShouldOpenInTab(extension);
// Opens the url as instructed by `open_in_tab`. On android we take a different
// path because the `Browser` object is not available.
// TODO(crbug.com/441209530): Unify the path on android after browser
// abstraction is introduced.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return WindowControllerFromBrowser(chrome::FindBrowserWithTab(web_contents))
      ->OpenOptionsPage(extension, *url, open_in_tab);
#else
  content::OpenURLParams params(
      *url, content::Referrer(),
      open_in_tab ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                  : WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false);
  web_contents->OpenURL(params, {});
  return true;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

// static
WindowController* ExtensionTabUtil::GetWindowControllerOfTab(
    WebContents* web_contents) {
  BrowserWindowInterface* browser =
      browser_window_util::GetBrowserForTabContents(*web_contents);
  if (browser) {
    return BrowserExtensionWindowController::From(browser);
  }
  return nullptr;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// static
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
  BrowserWindowInterface* browser = chrome::FindBrowserWithProfile(profile);
  if (!browser)
    browser = CreateBrowser(profile, true);
  if (!browser)
    return false;
  return extensions::ExtensionTabUtil::OpenOptionsPage(extension, browser);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// static
bool ExtensionTabUtil::OpenOptionsPage(const Extension* extension,
                                       BrowserWindowInterface* browser) {
  const std::optional<GURL> url = GetOptionsPageUrlToNavigate(extension);
  if (!url) {
    return false;
  }
  const bool open_in_tab = ShouldOpenInTab(extension);
  return WindowControllerFromBrowser(browser)->OpenOptionsPage(extension, *url,
                                                               open_in_tab);
}

// static
bool ExtensionTabUtil::BrowserSupportsTabs(BrowserWindowInterface* browser) {
  if (!browser) {
    return false;
  }

  // On non-android platforms, devtools windows are backed by a Browser
  // instance.
#if !BUILDFLAG(IS_ANDROID)
  // TODO(devlin): Should we be checking for other types, too? Like PiP?
  if (browser->GetType() == BrowserWindowInterface::TYPE_DEVTOOLS) {
    return false;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  return true;
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
  if (g_disable_tab_list_editing_for_testing) {
    return false;
  }

  // TODO(https://crbug.com/482088886): Migrate this to just use
  // TabListInterface::CanEditTabList().

  // See comments in the header for why we need to check all of them.
  for (WindowController* window : *WindowControllerList::GetInstance()) {
    BrowserWindowInterface* browser_window_interface =
        window->GetBrowserWindowInterface();
    // browser_window_interface can be null for non-browser windows on ChromeOS.
    if (!browser_window_interface) {
      continue;
    }
    TabListInterface* tab_list =
        TabListInterface::From(browser_window_interface);
    if (tab_list && !tab_list->IsThisTabListEditable()) {
      return false;
    }
  }
  return true;
}

// static
TabListInterface* ExtensionTabUtil::GetEditableTabList(
    BrowserWindowInterface& browser) {
  if (!TabListInterface::CanEditTabList(*browser.GetProfile())) {
    return nullptr;
  }
  return TabListInterface::From(&browser);
}

// static
base::AutoReset<bool> ExtensionTabUtil::DisableTabListEditingForTesting() {
  return base::AutoReset<bool>(&g_disable_tab_list_editing_for_testing, true);
}

}  // namespace extensions
