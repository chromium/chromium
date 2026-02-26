// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/sessions/sessions_api.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/notimplemented.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/sessions/session_id.h"
#include "chrome/browser/extensions/api/tabs/windows_util.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sessions/core/live_tab_context.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "ui/base/mojom/window_show_state.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/callback_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/android_live_tab_context.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "ui/base/page_transition_types.h"
#else
#include "chrome/browser/ui/browser_live_tab_context.h"
#endif

#if BUILDFLAG(IS_ANDROID)
// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/RecentlyClosedEntriesManager_jni.h"
#include "chrome/android/chrome_jni_headers/RecentlyClosedWindowMetadata_jni.h"
#endif  // BUILDFLAG(IS_ANDROID)

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace windows = api::windows;

namespace {

namespace GetRecentlyClosed = api::sessions::GetRecentlyClosed;
namespace GetDevices = api::sessions::GetDevices;
namespace Restore = api::sessions::Restore;

const char kNoRecentlyClosedSessionsError[] =
    "There are no recently closed sessions.";
const char kInvalidSessionIdError[] = "Invalid session id: \"*\".";
const char kNoBrowserToRestoreSession[] =
    "There are no browser windows to restore the session.";
const char kSessionSyncError[] = "Synced sessions are not available.";
const char kRestoreInIncognitoError[] =
    "Can not restore sessions in incognito mode.";
const char kNoLiveTabContextError[] = "Unable to determine live tab context.";
const char kNoActiveTabError[] = "No active tab.";

#if BUILDFLAG(IS_ANDROID)
// Must match Java TabWindowManager.WINDOW_INVALID_ID.
constexpr int kInvalidWindowId = -1;
#endif  // BUILDFLAG(IS_ANDROID)

// Comparator function for use with std::sort that will sort sessions by
// descending modified_time (i.e., most recent first).
bool SortSessionsByRecency(const sync_sessions::SyncedSession* s1,
                           const sync_sessions::SyncedSession* s2) {
  return s1->GetModifiedTime() > s2->GetModifiedTime();
}

// Comparator function for use with std::sort that will sort tabs in a window
// by descending timestamp (i.e., most recent first).
bool SortTabsByRecency(const sessions::SessionTab* t1,
                       const sessions::SessionTab* t2) {
  return t1->timestamp > t2->timestamp;
}

#if BUILDFLAG(IS_ANDROID)
// Comparator function for use with std::sort that will sort saved API session
// entries by descending timestamp (i.e., most recent first).
bool SortApiSessionsByRecency(const api::sessions::Session& s1,
                              const api::sessions::Session& s2) {
  return s1.last_modified > s2.last_modified;
}
#endif  // BUILDFLAG(IS_ANDROID)

// Creates an extensions tab API object. Takes primitive types as parameters
// so it can be used both with TabRestoreService (on Win/Mac/Linux) and
// TabModel (on Android).
api::tabs::Tab CreateTabModelHelper(const GURL& virtual_url,
                                    const std::u16string& title_utf16,
                                    const GURL& favicon_url,
                                    const std::string& session_id,
                                    int index,
                                    bool pinned,
                                    bool active,
                                    const Extension* extension,
                                    mojom::ContextType context) {
  api::tabs::Tab tab_struct;

  const GURL& url = virtual_url;
  std::string title = base::UTF16ToUTF8(title_utf16);

  tab_struct.session_id = session_id;
  tab_struct.url = url.spec();
  tab_struct.fav_icon_url = favicon_url.spec();
  if (!title.empty()) {
    tab_struct.title = title;
  } else {
    tab_struct.title = base::UTF16ToUTF8(url_formatter::FormatUrl(url));
  }
  tab_struct.index = index;
  tab_struct.pinned = pinned;
  tab_struct.active = active;

  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(extension, context, url);
  ExtensionTabUtil::ScrubTabForExtension(extension, nullptr, &tab_struct,
                                         scrub_tab_behavior);
  return tab_struct;
}

api::windows::Window CreateWindowModelHelper(
    std::vector<api::tabs::Tab> tabs,
    const std::string& session_id,
    const api::windows::WindowType& type,
    const api::windows::WindowState& state) {
  api::windows::Window window_struct;
  window_struct.tabs = std::move(tabs);
  window_struct.session_id = session_id;
  window_struct.incognito = false;
  window_struct.always_on_top = false;
  window_struct.focused = false;
  window_struct.type = type;
  window_struct.state = state;
  return window_struct;
}

// `last_modified` is in seconds from epoch.
api::sessions::Session CreateSessionModelHelper(
    int last_modified,
    std::optional<api::tabs::Tab> tab,
    std::optional<api::windows::Window> window,
    std::optional<api::tab_groups::TabGroup> group) {
  api::sessions::Session session_struct;
  session_struct.last_modified = last_modified;
  if (tab) {
    session_struct.tab = std::move(*tab);
  } else if (window) {
    session_struct.window = std::move(*window);
  } else if (group) {
    // TODO(crbug.com/40757179): Implement group support.
    NOTIMPLEMENTED();
  } else {
    NOTREACHED();
  }
  return session_struct;
}

bool is_window_entry(const sessions::tab_restore::Entry& entry) {
  return entry.type == sessions::tab_restore::Type::WINDOW;
}

// Returns the active web contents or null if none is found.
content::WebContents* GetActiveWebContents(BrowserWindowInterface* browser) {
  CHECK(browser);
  tabs::TabInterface* active_tab =
      TabListInterface::From(browser)->GetActiveTab();
  return active_tab ? active_tab->GetContents() : nullptr;
}

// Returns the `LiveTabContent` for a browser. The lookup method depends on
// the platform. Returns null if none is found.
sessions::LiveTabContext* GetLiveTabContextForBrowser(
    BrowserWindowInterface* browser) {
  CHECK(browser);
  auto* web_contents = GetActiveWebContents(browser);
  if (!web_contents) {
    return nullptr;
  }
#if BUILDFLAG(IS_ANDROID)
  return AndroidLiveTabContext::FindContextForWebContents(web_contents);
#else
  return BrowserLiveTabContext::FindContextForWebContents(web_contents);
#endif
}

// Returns the `BrowserWindowInterface` for a `Profile` or null if none is
// found.
BrowserWindowInterface* FindBrowserWindowInterfaceWithProfile(
    Profile* profile) {
  for (BrowserWindowInterface* bwi : GetAllBrowserWindowInterfaces()) {
    if (bwi->GetProfile() == profile) {
      return bwi;
    }
  }
  return nullptr;
}

#if BUILDFLAG(IS_ANDROID)
// Updates tab properties for `new_tab` in `new_tab_list` using data from
// `saved_tab`. There aren't many properties to update because properties like
// URL, title, favicon, etc. come from loading the page in the tab.
void UpdateTabState(TabAndroid* saved_tab,
                    TabListInterface* new_tab_list,
                    tabs::TabHandle new_tab) {
  if (saved_tab->IsPinned()) {
    new_tab_list->PinTab(new_tab);
  }
  if (saved_tab->IsActivated()) {
    new_tab_list->ActivateTab(new_tab);
  }
}

// Uses JNI to unpack a Java RecentlyClosedWindowMetadata object.
void UnpackRecentlyClosedWindowMetadata(
    const base::android::JavaRef<jobject>& j_tab_model_and_timestamp,
    base::android::ScopedJavaLocalRef<jobject>* j_tab_model,
    int64_t* timestamp,
    int* instance_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  *j_tab_model = Java_RecentlyClosedWindowMetadata_getTabModel(
      env, j_tab_model_and_timestamp);
  *timestamp = Java_RecentlyClosedWindowMetadata_getTimestamp(
      env, j_tab_model_and_timestamp);
  *instance_id = Java_RecentlyClosedWindowMetadata_getInstanceId(
      env, j_tab_model_and_timestamp);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

SessionsGetRecentlyClosedFunction::SessionsGetRecentlyClosedFunction() =
    default;

SessionsGetRecentlyClosedFunction::~SessionsGetRecentlyClosedFunction() =
    default;

api::tabs::Tab SessionsGetRecentlyClosedFunction::CreateTabModel(
    const sessions::tab_restore::Tab& tab,
    bool active) {
  const sessions::SerializedNavigationEntry& navigation =
      tab.navigations[tab.current_navigation_index];
  return CreateTabModelHelper(
      navigation.virtual_url(), navigation.title(), navigation.favicon_url(),
      base::NumberToString(tab.id.id()), tab.tabstrip_index, tab.pinned, active,
      extension(), source_context_type());
}

api::windows::Window SessionsGetRecentlyClosedFunction::CreateWindowModel(
    const sessions::tab_restore::Window& window) {
  DCHECK(!window.tabs.empty());

  std::vector<api::tabs::Tab> tabs;
  for (const auto& tab : window.tabs) {
    tabs.push_back(
        CreateTabModel(*tab, tab->tabstrip_index == window.selected_tab_index));
  }

  return CreateWindowModelHelper(
      std::move(tabs), base::NumberToString(window.id.id()),
      api::windows::WindowType::kNormal, api::windows::WindowState::kNormal);
}

api::tab_groups::TabGroup SessionsGetRecentlyClosedFunction::CreateGroupModel(
    const sessions::tab_restore::Group& group) {
  DCHECK(!group.tabs.empty());

  return ExtensionTabUtil::CreateTabGroupObject(group.group_id,
                                                group.visual_data);
}

api::sessions::Session SessionsGetRecentlyClosedFunction::CreateSessionModel(
    const sessions::tab_restore::Entry& entry) {
  std::optional<api::tabs::Tab> tab;
  std::optional<api::windows::Window> window;
  std::optional<api::tab_groups::TabGroup> group;
  switch (entry.type) {
    case sessions::tab_restore::Type::TAB:
      tab = CreateTabModel(
          static_cast<const sessions::tab_restore::Tab&>(entry), false);
      break;
    case sessions::tab_restore::Type::WINDOW:
      window = CreateWindowModel(
          static_cast<const sessions::tab_restore::Window&>(entry));
      break;
    case sessions::tab_restore::Type::GROUP:
      group = CreateGroupModel(
          static_cast<const sessions::tab_restore::Group&>(entry));
  }
  return CreateSessionModelHelper(entry.timestamp.ToTimeT(), std::move(tab),
                                  std::move(window), std::move(group));
}

ExtensionFunction::ResponseAction SessionsGetRecentlyClosedFunction::Run() {
  std::optional<GetRecentlyClosed::Params> params =
      GetRecentlyClosed::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  if (params->filter && params->filter->max_results) {
    max_results_ = *params->filter->max_results;
  }
  EXTENSION_FUNCTION_VALIDATE(
      max_results_ <= static_cast<size_t>(api::sessions::MAX_SESSION_RESULTS));

  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));

  // TabRestoreServiceFactory::GetForProfile() can return nullptr (i.e., when in
  // incognito mode)
  if (!tab_restore_service) {
    DCHECK(browser_context()->IsOffTheRecord())
        << "sessions::TabRestoreService expected for normal profiles";
    return RespondNow(
        ArgumentList(GetRecentlyClosed::Results::Create(result_)));
  }

  // List of entries. They are ordered from most to least recent.
  // We prune the list to contain max 25 entries at any time and removes
  // uninteresting entries.
  for (const auto& entry : tab_restore_service->entries()) {
    // TODO(crbug.com/40757179): Support group entries in the Sessions API,
    // rather than sharding the group out into individual tabs.
    if (entry->type == sessions::tab_restore::Type::GROUP) {
      auto& group = static_cast<const sessions::tab_restore::Group&>(*entry);
      for (const auto& tab : group.tabs) {
        if (result_.size() < max_results_) {
          result_.push_back(CreateSessionModel(*tab));
        } else {
          break;
        }
      }
    } else {
      if (result_.size() < max_results_) {
        result_.push_back(CreateSessionModel(*entry));
      } else {
        break;
      }
    }
  }

#if BUILDFLAG(IS_ANDROID)
  // On Android stores window information on the Java side, so call through JNI
  // to fetch any recently closed window. Window state may not be persisted at
  // the time of the call, so pass a callback that will be invoked when window
  // state is available. The callback is invoked with null on error and when
  // there is no window available.
  JNIEnv* env = base::android::AttachCurrentThread();
  // Don't filter by instance id.
  constexpr int instance_id = kInvalidWindowId;
  base::OnceCallback<void(const base::android::JavaRef<jobject>&)> j_callback =
      base::BindOnce(
          &SessionsGetRecentlyClosedFunction::OnGetRecentlyClosedWindow, this);
  Java_RecentlyClosedEntriesManager_getRecentlyClosedWindow(
      env, instance_id,
      base::android::ToJniCallback(env, std::move(j_callback)));
  if (did_respond()) {
    // The callback may be invoked immediately for errors, in which case
    // we have already responded.
    return AlreadyResponded();
  } else {
    // Otherwise we will respond in OnGetRecentlyClosedWindow().
    return RespondLater();
  }
#else
  // On Win/Mac/Linux the TabRestoreService has window information, so we can
  // respond immediately.
  return RespondNow(ArgumentList(GetRecentlyClosed::Results::Create(result_)));
#endif  // BUILDFLAG(IS_ANDROID)
}

#if BUILDFLAG(IS_ANDROID)
void SessionsGetRecentlyClosedFunction::OnGetRecentlyClosedWindow(
    const base::android::JavaRef<jobject>& j_tab_model_and_timestamp) {
  if (j_tab_model_and_timestamp.is_null()) {
    // Error on the Java side, so no valid window to add.
    Respond(ArgumentList(GetRecentlyClosed::Results::Create(result_)));
    return;
  }

  // Unpack the Java object.
  base::android::ScopedJavaLocalRef<jobject> j_tab_model;
  int64_t timestamp = 0;
  int instance_id = kInvalidWindowId;
  UnpackRecentlyClosedWindowMetadata(j_tab_model_and_timestamp, &j_tab_model,
                                     &timestamp, &instance_id);

  if (j_tab_model.is_null()) {
    // No tab model, so no valid window to add.
    Respond(ArgumentList(GetRecentlyClosed::Results::Create(result_)));
    return;
  }

  // Look up the C++ side TabModel.
  TabModel* model = TabModelList::FindNativeTabModelForJavaObject(j_tab_model);
  if (!model) {
    Respond(ArgumentList(GetRecentlyClosed::Results::Create(result_)));
    return;
  }

  // Extract the URL for the closed windows.
  std::vector<api::tabs::Tab> api_tabs;
  for (int index = 0; index < model->GetTabCount(); ++index) {
    TabAndroid* tab = model->GetTabAt(index);
    CHECK(tab);
    // NOTE: The tabs may not have WebContents, since the window is closed.
    // TODO(crbug.com/405219627): Figure out how to pass the favicon URL.
    api::tabs::Tab api_tab = CreateTabModelHelper(
        tab->GetURL(), tab->GetTitle(), /*favicon_url=*/GURL(),
        base::NumberToString(tab->GetWindowId().id()), index, tab->IsPinned(),
        tab->IsActivated(), extension(), source_context_type());
    api_tabs.push_back(std::move(api_tab));
  }

  // Populate the window and session objects. Android uses Chrome Activity
  // instance ids instead of session ids because that's what the
  // RecentlyClosedEntriesManager tracks. They are stable across the browsing
  // session and work fine as a session ID replacement.
  std::string session_id = base::NumberToString(instance_id);
  api::windows::Window window = CreateWindowModelHelper(
      std::move(api_tabs), session_id, api::windows::WindowType::kNormal,
      api::windows::WindowState::kNormal);

  // The timestamp from Java is in milliseconds, last_modified is in seconds.
  int last_modified = window_last_modified_for_test_
                          ? window_last_modified_for_test_
                          : timestamp / 1000;
  api::sessions::Session session = CreateSessionModelHelper(
      last_modified, std::nullopt, std::move(window), std::nullopt);

  // Add the session to the result.
  result_.push_back(std::move(session));

  // The window close might have happened more recently than the tab closures.
  // Ensure the results are sorted by timestamp.
  std::sort(result_.begin(), result_.end(), SortApiSessionsByRecency);

  // Adding the window may have pushed us over our result limit. Restrict to
  // the most recent results.
  if (result_.size() > max_results_) {
    result_.resize(max_results_);
  }

  // Respond to the API caller.
  Respond(ArgumentList(GetRecentlyClosed::Results::Create(result_)));
}
#endif  // BUILDFLAG(IS_ANDROID)

api::tabs::Tab SessionsGetDevicesFunction::CreateTabModel(
    const std::string& session_tag,
    const sessions::SessionTab& tab,
    int tab_index,
    bool active) {
  std::string session_id = SessionId(session_tag, tab.tab_id.id()).ToString();
  const sessions::SerializedNavigationEntry& navigation =
      tab.navigations[tab.normalized_navigation_index()];
  return CreateTabModelHelper(navigation.virtual_url(), navigation.title(),
                              navigation.favicon_url(), session_id, tab_index,
                              tab.pinned, active, extension(),
                              source_context_type());
}

std::optional<api::windows::Window>
SessionsGetDevicesFunction::CreateWindowModel(
    const sessions::SessionWindow& window,
    const std::string& session_tag) {
  DCHECK(!window.tabs.empty());

  // Ignore app popup window for now because we do not have a corresponding
  // api::windows::WindowType value.
  if (window.type == sessions::SessionWindow::TYPE_APP_POPUP) {
    return std::nullopt;
  }

  // Prune tabs that are not syncable or are NewTabPage. Then, sort the tabs
  // from most recent to least recent.
  std::vector<const sessions::SessionTab*> tabs_in_window;
  for (const auto& tab : window.tabs) {
    if (tab->navigations.empty()) {
      continue;
    }
    const sessions::SerializedNavigationEntry& current_navigation =
        tab->navigations.at(tab->normalized_navigation_index());
    if (search::IsNTPOrRelatedURL(
            current_navigation.virtual_url(),
            Profile::FromBrowserContext(browser_context()))) {
      continue;
    }
    tabs_in_window.push_back(tab.get());
  }
  if (tabs_in_window.empty()) {
    return std::nullopt;
  }
  std::sort(tabs_in_window.begin(), tabs_in_window.end(), SortTabsByRecency);

  std::vector<api::tabs::Tab> tabs;
  for (size_t i = 0; i < tabs_in_window.size(); ++i) {
    bool is_active = false;
    if (window.selected_tab_index != -1) {
      SessionID selected_tab_id =
          window.tabs[window.selected_tab_index]->tab_id;
      SessionID current_tab_id = tabs_in_window[i]->tab_id;
      is_active = selected_tab_id == current_tab_id;
    }
    tabs.push_back(
        CreateTabModel(session_tag, *tabs_in_window[i], i, is_active));
  }

  std::string session_id =
      SessionId(session_tag, window.window_id.id()).ToString();

  api::windows::WindowType type = api::windows::WindowType::kNone;
  switch (window.type) {
    case sessions::SessionWindow::TYPE_NORMAL:
      type = api::windows::WindowType::kNormal;
      break;
    case sessions::SessionWindow::TYPE_POPUP:
      type = api::windows::WindowType::kPopup;
      break;
    case sessions::SessionWindow::TYPE_APP:
      type = api::windows::WindowType::kApp;
      break;
    case sessions::SessionWindow::TYPE_DEVTOOLS:
      type = api::windows::WindowType::kDevtools;
      break;
    case sessions::SessionWindow::TYPE_APP_POPUP:
      NOTREACHED();
  }

  api::windows::WindowState state = api::windows::WindowState::kNone;
  switch (window.show_state) {
    case ui::mojom::WindowShowState::kNormal:
      state = api::windows::WindowState::kNormal;
      break;
    case ui::mojom::WindowShowState::kMinimized:
      state = api::windows::WindowState::kMinimized;
      break;
    case ui::mojom::WindowShowState::kMaximized:
      state = api::windows::WindowState::kMaximized;
      break;
    case ui::mojom::WindowShowState::kFullscreen:
      state = api::windows::WindowState::kFullscreen;
      break;
    case ui::mojom::WindowShowState::kDefault:
    case ui::mojom::WindowShowState::kInactive:
    case ui::mojom::WindowShowState::kEnd:
      break;
  }

  api::windows::Window window_struct =
      CreateWindowModelHelper(std::move(tabs), session_id, type, state);
  // TODO(dwankri): Dig deeper to resolve bounds not being optional, so closed
  // windows in GetRecentlyClosed can have set values in Window helper.
  window_struct.left = window.bounds.x();
  window_struct.top = window.bounds.y();
  window_struct.width = window.bounds.width();
  window_struct.height = window.bounds.height();

  return window_struct;
}

std::optional<api::sessions::Session>
SessionsGetDevicesFunction::CreateSessionModel(
    const sessions::SessionWindow& window,
    const std::string& session_tag) {
  std::optional<api::windows::Window> window_model =
      CreateWindowModel(window, session_tag);
  // There is a chance that after pruning uninteresting tabs the window will be
  // empty.
  if (!window_model) {
    return std::nullopt;
  }

  return CreateSessionModelHelper(window.timestamp.ToTimeT(), std::nullopt,
                                  std::move(window_model), std::nullopt);
}

api::sessions::Device SessionsGetDevicesFunction::CreateDeviceModel(
    const sync_sessions::SyncedSession* session) {
  int max_results = api::sessions::MAX_SESSION_RESULTS;
  // Already validated in RunAsync().
  std::optional<GetDevices::Params> params = GetDevices::Params::Create(args());
  if (params->filter && params->filter->max_results) {
    max_results = *params->filter->max_results;
  }

  api::sessions::Device device_struct;
  device_struct.info = session->GetSessionName();
  device_struct.device_name = session->GetSessionName();

  for (auto it = session->windows.begin();
       it != session->windows.end() &&
       static_cast<int>(device_struct.sessions.size()) < max_results;
       ++it) {
    std::optional<api::sessions::Session> session_model = CreateSessionModel(
        it->second->wrapped_window, session->GetSessionTag());
    if (session_model) {
      device_struct.sessions.push_back(std::move(*session_model));
    }
  }
  return device_struct;
}

ExtensionFunction::ResponseAction SessionsGetDevicesFunction::Run() {
  sync_sessions::SessionSyncService* service =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  DCHECK(service);

  sync_sessions::OpenTabsUIDelegate* open_tabs =
      service->GetOpenTabsUIDelegate();
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      sessions;
  // If the user has disabled tab sync, GetOpenTabsUIDelegate() returns null.
  if (!(open_tabs && open_tabs->GetAllForeignSessions(&sessions))) {
    return RespondNow(ArgumentList(
        GetDevices::Results::Create(std::vector<api::sessions::Device>())));
  }

  std::optional<GetDevices::Params> params = GetDevices::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  if (params->filter && params->filter->max_results) {
    EXTENSION_FUNCTION_VALIDATE(*params->filter->max_results >= 0 &&
                                *params->filter->max_results <=
                                    api::sessions::MAX_SESSION_RESULTS);
  }

  std::vector<api::sessions::Device> result;
  // Sort sessions from most recent to least recent.
  std::sort(sessions.begin(), sessions.end(), SortSessionsByRecency);
  for (const sync_sessions::SyncedSession* session : sessions) {
    result.push_back(CreateDeviceModel(session));
  }

  return RespondNow(ArgumentList(GetDevices::Results::Create(result)));
}

SessionsRestoreFunction::SessionsRestoreFunction() = default;

SessionsRestoreFunction::~SessionsRestoreFunction() = default;

ExtensionFunction::ResponseValue SessionsRestoreFunction::GetRestoredTabResult(
    content::WebContents* contents) {
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(extension(), source_context_type(),
                                            contents);
  api::tabs::Tab tab = ExtensionTabUtil::CreateTabObject(
      contents, scrub_tab_behavior, extension());
  api::sessions::Session restored_session = CreateSessionModelHelper(
      base::Time::Now().ToTimeT(), std::move(tab), std::nullopt, std::nullopt);
  return ArgumentList(Restore::Results::Create(restored_session));
}

ExtensionFunction::ResponseValue
SessionsRestoreFunction::GetRestoredWindowResult(int window_id) {
  WindowController* window_controller = nullptr;
  std::string error;
  if (!windows_util::GetControllerFromWindowID(this, window_id, 0,
                                               &window_controller, &error)) {
    return Error(error);
  }
  base::DictValue window_value =
      window_controller->CreateWindowValueForExtension(
          extension(), WindowController::kPopulateTabs, source_context_type());
  std::optional<api::windows::Window> window =
      api::windows::Window::FromValue(window_value);
  DCHECK(window);
  return ArgumentList(Restore::Results::Create(
      CreateSessionModelHelper(base::Time::Now().ToTimeT(), std::nullopt,
                               std::move(*window), std::nullopt)));
}

ExtensionFunction::ResponseAction
SessionsRestoreFunction::RestoreMostRecentlyClosed(
    BrowserWindowInterface* browser) {
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  const sessions::TabRestoreService::Entries& entries =
      tab_restore_service->entries();

  if (entries.empty()) {
#if BUILDFLAG(IS_ANDROID)
    // Android only stores tab restore information in TabRestoreService, so we
    // must also query the Java side to check for window restore information.
    // Use instance id of kInvalidWindowId so we don't filter and just get the
    // most recent.
    return QueryRecentlyClosedEntitiesManager(/*instance_id=*/kInvalidWindowId);
#else
    // Other platforms store everything in TabRestoreService, so if there are no
    // entries there is nothing to restore.
    return RespondNow(Error(kNoRecentlyClosedSessionsError));
#endif
  }

  bool is_window = is_window_entry(*entries.front());
  sessions::LiveTabContext* context = GetLiveTabContextForBrowser(browser);
  if (!context) {
    return RespondNow(Error(kNoLiveTabContextError));
  }
  std::vector<sessions::LiveTab*> restored_tabs =
      tab_restore_service->RestoreMostRecentEntry(context);
  DCHECK(restored_tabs.size());

  sessions::ContentLiveTab* first_tab =
      static_cast<sessions::ContentLiveTab*>(restored_tabs[0]);
  if (is_window) {
    return RespondNow(GetRestoredWindowResult(
        ExtensionTabUtil::GetWindowIdOfTab(&first_tab->GetWebContents())));
  }

  return RespondNow(GetRestoredTabResult(&first_tab->GetWebContents()));
}

#if BUILDFLAG(IS_ANDROID)
ExtensionFunction::ResponseAction
SessionsRestoreFunction::QueryRecentlyClosedEntitiesManager(int instance_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  // Getting recently closed windows from Java is asynchronous. `this` is safe
  // because `SessionsRestoreFunction` is ref-counted.
  base::OnceCallback<void(const base::android::JavaRef<jobject>&)> callback =
      base::BindOnce(&SessionsRestoreFunction::OnGetRecentlyClosedWindow, this);
  Java_RecentlyClosedEntriesManager_getRecentlyClosedWindow(
      env, instance_id, base::android::ToJniCallback(env, std::move(callback)));

  // Check if the callback already ran and responded to the extension.
  if (did_respond()) {
    return AlreadyResponded();
  } else {
    return RespondLater();
  }
}

void SessionsRestoreFunction::OnGetRecentlyClosedWindow(
    const base::android::JavaRef<jobject>& j_tab_model_and_timestamp) {
  if (j_tab_model_and_timestamp.is_null()) {
    // Error on the Java side, so no valid window to restore.
    Respond(Error(kNoRecentlyClosedSessionsError));
    return;
  }

  // Unpack the Java object.
  base::android::ScopedJavaLocalRef<jobject> j_tab_model;
  int64_t timestamp = 0;
  int instance_id = kInvalidWindowId;
  UnpackRecentlyClosedWindowMetadata(j_tab_model_and_timestamp, &j_tab_model,
                                     &timestamp, &instance_id);

  if (j_tab_model.is_null()) {
    // No tab model, so no window to restore.
    Respond(Error(kNoRecentlyClosedSessionsError));
    return;
  }

  // Look up the C++ side TabModel.
  TabModel* saved_tab_model =
      TabModelList::FindNativeTabModelForJavaObject(j_tab_model);
  if (!saved_tab_model) {
    // No tab model, so no window to restore.
    Respond(Error(kNoRecentlyClosedSessionsError));
    return;
  }

  // Ensure there are tabs in the window to restore.
  if (saved_tab_model->GetTabCount() == 0) {
    Respond(Error(kNoRecentlyClosedSessionsError));
    return;
  }

  // Save the tab model object, which we'll need after window creation. This is
  // a Java global reference, so the object will stay alive across the callback.
  global_ref_tab_model_ = j_tab_model;

  // Open a window, which is asynchronous.
  // TODO(crbug.com/405219627): Restore window bounds.
  BrowserWindowCreateParams params(
      BrowserWindowInterface::TYPE_NORMAL,
      *Profile::FromBrowserContext(browser_context()),
      /*from_user_gesture=*/false);
  // `this` is safe because the object is ref-counted.
  auto callback =
      base::BindOnce(&SessionsRestoreFunction::OnBrowserWindowCreated, this);
  CreateBrowserWindow(std::move(params), std::move(callback));
}

void SessionsRestoreFunction::OnBrowserWindowCreated(
    BrowserWindowInterface* browser) {
  TabListInterface* new_tab_list = TabListInterface::From(browser);
  CHECK(new_tab_list);

  // Look up the C++ side TabModel again.
  TabModel* saved_tab_model =
      TabModelList::FindNativeTabModelForJavaObject(global_ref_tab_model_);
  if (!saved_tab_model) {
    Respond(Error(kNoRecentlyClosedSessionsError));
    return;
  }

  // This should not happen, but just in case.
  if (saved_tab_model->GetTabCount() == 0) {
    Respond(Error(kNoRecentlyClosedSessionsError));
    return;
  }

  // New Android browser windows start with one tab open already. Load the first
  // URL into that tab's WebContents.
  CHECK_EQ(new_tab_list->GetTabCount(), 1);
  content::WebContents* first_contents = new_tab_list->GetTab(0)->GetContents();
  CHECK(first_contents);
  GURL first_url = saved_tab_model->GetTabAt(0)->GetURL();
  base::WeakPtr<content::NavigationHandle> handle =
      first_contents->GetController().LoadURL(first_url, content::Referrer(),
                                              ui::PAGE_TRANSITION_FROM_API,
                                              /*extra_headers=*/std::string());

  // This should not happen, but just in case.
  if (!handle.get()) {
    Respond(Error(kNoRecentlyClosedSessionsError));
    return;
  }

  // Create new tabs for the rest of the saved tabs.
  for (int i = 1; i < saved_tab_model->GetTabCount(); ++i) {
    TabAndroid* saved_tab = saved_tab_model->GetTabAt(i);
    CHECK(saved_tab);
    new_tab_list->OpenTab(saved_tab->GetURL(), i);
  }

  // Update tab state like pinned and active in a separate loop because loading
  // new tabs may change activation state.
  for (int i = 0; i < saved_tab_model->GetTabCount(); ++i) {
    TabAndroid* saved_tab = saved_tab_model->GetTabAt(i);
    tabs::TabHandle new_tab_handle = new_tab_list->GetTab(i)->GetHandle();
    UpdateTabState(saved_tab, new_tab_list, new_tab_handle);
  }

  // Respond to the API. Note that the tabs have not yet finished loading, so
  // their "committed URL" will be empty. They will eventually finish and we
  // don't want to block the API on multiple tab loads.
  Respond(GetRestoredWindowResult(ExtensionTabUtil::GetWindowId(browser)));
}
#endif  // BUILDFLAG(IS_ANDROID)

ExtensionFunction::ResponseAction SessionsRestoreFunction::RestoreLocalSession(
    const SessionId& session_id,
    BrowserWindowInterface* browser) {
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  const sessions::TabRestoreService::Entries& entries =
      tab_restore_service->entries();

  if (entries.empty()) {
#if BUILDFLAG(IS_ANDROID)
    // Android only stores tab restore information in TabRestoreService, so we
    // must also query the Java side to check for window restore information.
    return QueryRecentlyClosedEntitiesManager(session_id.id());
#else
    // Other platforms store everything in TabRestoreService, so if there are no
    // entries there is nothing matching that session id.
    return RespondNow(Error(kInvalidSessionIdError, session_id.ToString()));
#endif
  }

  // Check if the recently closed list contains an entry with the provided id.
  bool is_window = false;
  for (const auto& entry : entries) {
    if (entry->id.id() == session_id.id()) {
      // A full window is being restored only if the entry ID
      // matches the provided ID and the entry type is Window.
      is_window = is_window_entry(*entry);
      break;
    }
  }

  sessions::LiveTabContext* context = GetLiveTabContextForBrowser(browser);
  if (!context) {
    return RespondNow(Error(kNoLiveTabContextError));
  }
  std::vector<sessions::LiveTab*> restored_tabs =
      tab_restore_service->RestoreEntryById(
          context, SessionID::FromSerializedValue(session_id.id()),
          WindowOpenDisposition::UNKNOWN);
  // If the ID is invalid, restored_tabs will be empty.
  if (restored_tabs.empty()) {
    return RespondNow(Error(kInvalidSessionIdError, session_id.ToString()));
  }

  sessions::ContentLiveTab* first_tab =
      static_cast<sessions::ContentLiveTab*>(restored_tabs[0]);

  // Retrieve the window through any of the tabs in restored_tabs.
  if (is_window) {
    return RespondNow(GetRestoredWindowResult(
        ExtensionTabUtil::GetWindowIdOfTab(&first_tab->GetWebContents())));
  }

  return RespondNow(GetRestoredTabResult(&first_tab->GetWebContents()));
}

ExtensionFunction::ResponseAction
SessionsRestoreFunction::RestoreForeignSession(
    const SessionId& session_id,
    BrowserWindowInterface* browser) {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  sync_sessions::SessionSyncService* service =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile);
  DCHECK(service);

  sync_sessions::OpenTabsUIDelegate* open_tabs =
      service->GetOpenTabsUIDelegate();
  // If the user has disabled tab sync, GetOpenTabsUIDelegate() returns null.
  if (!open_tabs) {
    return RespondNow(Error(kSessionSyncError));
  }

  const sessions::SessionTab* tab = nullptr;
  if (open_tabs->GetForeignTab(session_id.session_tag(),
                               SessionID::FromSerializedValue(session_id.id()),
                               &tab)) {
    content::WebContents* contents = GetActiveWebContents(browser);
    if (!contents) {
      return RespondNow(Error(kNoActiveTabError));
    }

    content::WebContents* tab_contents =
        SessionRestore::RestoreForeignSessionTab(
            contents, *tab, WindowOpenDisposition::NEW_FOREGROUND_TAB);
    return RespondNow(GetRestoredTabResult(tab_contents));
  }

  // Restoring a full window.
  std::vector<const sessions::SessionWindow*> windows =
      open_tabs->GetForeignSession(session_id.session_tag());
  if (windows.empty()) {
    return RespondNow(Error(kInvalidSessionIdError, session_id.ToString()));
  }

  std::vector<const sessions::SessionWindow*>::const_iterator window =
      windows.begin();
  while (window != windows.end() &&
         (*window)->window_id.id() != session_id.id()) {
    ++window;
  }
  if (window == windows.end()) {
    return RespondNow(Error(kInvalidSessionIdError, session_id.ToString()));
  }

  // Only restore one window at a time.
  SessionRestore::RestoreForeignSessionWindows(
      profile, window, window + 1,
      base::BindOnce(&SessionsRestoreFunction::OnRestoreForeignSessionWindows,
                     this));
  // Window restore is asynchronous on Android but synchronous on Win/Mac/Linux.
  // On Win/Mac/Linux we may have already called the callback and responded. On
  // Android we need to respond later.
  if (did_respond()) {
    return AlreadyResponded();
  } else {
    return RespondLater();
  }
}

void SessionsRestoreFunction::OnRestoreForeignSessionWindows(
    std::vector<BrowserWindowInterface*> browsers) {
  // Will always create one browser because we only restore one window per call.
  DCHECK_EQ(1u, browsers.size());
  Respond(GetRestoredWindowResult(ExtensionTabUtil::GetWindowId(browsers[0])));
}

ExtensionFunction::ResponseAction SessionsRestoreFunction::Run() {
  std::optional<Restore::Params> params = Restore::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  BrowserWindowInterface* browser =
      FindBrowserWindowInterfaceWithProfile(profile);
  if (!browser) {
    return RespondNow(Error(kNoBrowserToRestoreSession));
  }

  if (profile != profile->GetOriginalProfile()) {
    return RespondNow(Error(kRestoreInIncognitoError));
  }

  if (!ExtensionTabUtil::IsTabStripEditable()) {
    return RespondNow(Error(ExtensionTabUtil::kTabStripNotEditableError));
  }

  if (!params->session_id) {
    return RestoreMostRecentlyClosed(browser);
  }

  std::unique_ptr<SessionId> session_id(SessionId::Parse(*params->session_id));
  if (!session_id) {
    return RespondNow(Error(kInvalidSessionIdError, *params->session_id));
  }

  if (!session_id->IsForeign()) {
    return RestoreLocalSession(*session_id, browser);
  }

  // Foreign window restore is sometimes asynchronous, so it may return
  // RespondLater().
  return RestoreForeignSession(*session_id, browser);
}

SessionsEventRouter::SessionsEventRouter(Profile* profile)
    : profile_(profile),
      tab_restore_service_(TabRestoreServiceFactory::GetForProfile(profile)) {
  CHECK(profile_);
  // TabRestoreServiceFactory::GetForProfile() can return nullptr (i.e., when in
  // incognito mode)
  if (tab_restore_service_) {
    tab_restore_service_->LoadTabsFromLastSession();
    tab_restore_service_->AddObserver(this);
  }

#if BUILDFLAG(IS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  // Unretained is safe because the callback is cleared during destruction.
  auto callback = base::BindRepeating(
      &SessionsEventRouter::OnRecentlyClosedUpdated, base::Unretained(this));
  // Register a callback for updates to Java RecentlyClosedEntriesManager.
  // Limit the updates to the current profile.
  Java_RecentlyClosedEntriesManager_setNativeUpdatedCallback(
      env, profile_, base::android::ToJniCallback(env, std::move(callback)));
#endif  // BUILDFLAG(IS_ANDROID)
}

SessionsEventRouter::~SessionsEventRouter() {
  if (tab_restore_service_) {
    tab_restore_service_->RemoveObserver(this);
  }

#if BUILDFLAG(IS_ANDROID)
  // Clear the Java callback for this profile.
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_RecentlyClosedEntriesManager_setNativeUpdatedCallback(env, profile_,
                                                             nullptr);
#endif  // BUILDFLAG(IS_ANDROID)
}

void SessionsEventRouter::TabRestoreServiceChanged(
    sessions::TabRestoreService* service) {
  BroadcastOnChangedEvent(profile_);
}

void SessionsEventRouter::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {
  tab_restore_service_ = nullptr;
}

#if BUILDFLAG(IS_ANDROID)
void SessionsEventRouter::OnRecentlyClosedUpdated(int64_t j_browser_context) {
  content::BrowserContext* browser_context =
      reinterpret_cast<content::BrowserContext*>(j_browser_context);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  // If something went wrong on the Java side, don't broadcast.
  if (!profile) {
    return;
  }
  BroadcastOnChangedEvent(profile);
}
#endif  // BUILDFLAG(IS_ANDROID)

// static
void SessionsEventRouter::BroadcastOnChangedEvent(Profile* profile) {
  CHECK(profile);
  EventRouter::Get(profile)->BroadcastEvent(std::make_unique<Event>(
      events::SESSIONS_ON_CHANGED, api::sessions::OnChanged::kEventName,
      base::ListValue()));
}

SessionsAPI::SessionsAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter::Get(browser_context_)
      ->RegisterObserver(this, api::sessions::OnChanged::kEventName);
}

SessionsAPI::~SessionsAPI() = default;

void SessionsAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<SessionsAPI>>::
    DestructorAtExit g_sessions_api_factory = LAZY_INSTANCE_INITIALIZER;

BrowserContextKeyedAPIFactory<SessionsAPI>* SessionsAPI::GetFactoryInstance() {
  return g_sessions_api_factory.Pointer();
}

void SessionsAPI::OnListenerAdded(const EventListenerInfo& details) {
  sessions_event_router_ = std::make_unique<SessionsEventRouter>(
      Profile::FromBrowserContext(browser_context_));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

}  // namespace extensions
