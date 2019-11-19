// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/sessions/sessions_api.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
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
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/error_utils.h"
#include "ui/base/layout.h"

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

// Comparator function for use with std::sort that will sort sessions by
// descending modified_time (i.e., most recent first).
bool SortSessionsByRecency(const sync_sessions::SyncedSession* s1,
                           const sync_sessions::SyncedSession* s2) {
  return s1->modified_time > s2->modified_time;
}

// Comparator function for use with std::sort that will sort tabs in a window
// by descending timestamp (i.e., most recent first).
bool SortTabsByRecency(const sessions::SessionTab* t1,
                       const sessions::SessionTab* t2) {
  return t1->timestamp > t2->timestamp;
}

api::tabs::Tab CreateTabModelHelper(
    const sessions::SerializedNavigationEntry& current_navigation,
    const std::string& session_id,
    int index,
    bool pinned,
    bool active,
    const Extension* extension,
    Feature::Context context) {
  api::tabs::Tab tab_struct;

  const GURL& url = current_navigation.virtual_url();
  std::string title = base::UTF16ToUTF8(current_navigation.title());

  tab_struct.session_id.reset(new std::string(session_id));
  tab_struct.url.reset(new std::string(url.spec()));
  tab_struct.fav_icon_url.reset(
      new std::string(current_navigation.favicon_url().spec()));
  if (!title.empty()) {
    tab_struct.title.reset(new std::string(title));
  } else {
    tab_struct.title.reset(new std::string(
        base::UTF16ToUTF8(url_formatter::FormatUrl(url))));
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

std::unique_ptr<api::windows::Window> CreateWindowModelHelper(
    std::unique_ptr<std::vector<api::tabs::Tab>> tabs,
    const std::string& session_id,
    const api::windows::WindowType& type,
    const api::windows::WindowState& state) {
  std::unique_ptr<api::windows::Window> window_struct(new api::windows::Window);
  window_struct->tabs = std::move(tabs);
  window_struct->session_id.reset(new std::string(session_id));
  window_struct->incognito = false;
  window_struct->always_on_top = false;
  window_struct->focused = false;
  window_struct->type = type;
  window_struct->state = state;
  return window_struct;
}

std::unique_ptr<api::sessions::Session> CreateSessionModelHelper(
    int last_modified,
    std::unique_ptr<api::tabs::Tab> tab,
    std::unique_ptr<api::windows::Window> window) {
  std::unique_ptr<api::sessions::Session> session_struct(
      new api::sessions::Session());
  session_struct->last_modified = last_modified;
  if (tab)
    session_struct->tab = std::move(tab);
  else if (window)
    session_struct->window = std::move(window);
  else
    NOTREACHED();
  return session_struct;
}

bool is_window_entry(const sessions::TabRestoreService::Entry& entry) {
  return entry.type == sessions::TabRestoreService::WINDOW;
}

}  // namespace

api::tabs::Tab SessionsGetRecentlyClosedFunction::CreateTabModel(
    const sessions::TabRestoreService::Tab& tab,
    bool active) {
  return CreateTabModelHelper(tab.navigations[tab.current_navigation_index],
                              base::NumberToString(tab.id.id()),
                              tab.tabstrip_index, tab.pinned, active,
                              extension(), source_context_type());
}

std::unique_ptr<api::windows::Window>
SessionsGetRecentlyClosedFunction::CreateWindowModel(
    const sessions::TabRestoreService::Window& window) {
  DCHECK(!window.tabs.empty());

  auto tabs = std::make_unique<std::vector<api::tabs::Tab>>();
  for (const auto& tab : window.tabs)
    tabs->push_back(
        CreateTabModel(*tab, tab->tabstrip_index == window.selected_tab_index));

  return CreateWindowModelHelper(
      std::move(tabs), base::NumberToString(window.id.id()),
      api::windows::WINDOW_TYPE_NORMAL, api::windows::WINDOW_STATE_NORMAL);
}

std::unique_ptr<api::sessions::Session>
SessionsGetRecentlyClosedFunction::CreateSessionModel(
    const sessions::TabRestoreService::Entry& entry) {
  std::unique_ptr<api::tabs::Tab> tab;
  std::unique_ptr<api::windows::Window> window;
  switch (entry.type) {
    case sessions::TabRestoreService::TAB:
      tab.reset(new api::tabs::Tab(CreateTabModel(
          static_cast<const sessions::TabRestoreService::Tab&>(entry), false)));
      break;
    case sessions::TabRestoreService::WINDOW:
      window = CreateWindowModel(
          static_cast<const sessions::TabRestoreService::Window&>(entry));
      break;
    default:
      NOTREACHED();
  }
  return CreateSessionModelHelper(entry.timestamp.ToTimeT(), std::move(tab),
                                  std::move(window));
}

ExtensionFunction::ResponseAction SessionsGetRecentlyClosedFunction::Run() {
  std::unique_ptr<GetRecentlyClosed::Params> params(
      GetRecentlyClosed::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  int max_results = api::sessions::MAX_SESSION_RESULTS;
  if (params->filter && params->filter->max_results)
    max_results = *params->filter->max_results;
  EXTENSION_FUNCTION_VALIDATE(max_results >= 0 &&
      max_results <= api::sessions::MAX_SESSION_RESULTS);

  std::vector<api::sessions::Session> result;
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));

  // TabRestoreServiceFactory::GetForProfile() can return NULL (i.e., when in
  // incognito mode)
  if (!tab_restore_service) {
    DCHECK(browser_context()->IsOffTheRecord())
        << "sessions::TabRestoreService expected for normal profiles";
    return RespondNow(ArgumentList(GetRecentlyClosed::Results::Create(result)));
  }

  // List of entries. They are ordered from most to least recent.
  // We prune the list to contain max 25 entries at any time and removes
  // uninteresting entries.
  for (const auto& entry : tab_restore_service->entries()) {
    result.push_back(std::move(*CreateSessionModel(*entry)));
  }

  return RespondNow(ArgumentList(GetRecentlyClosed::Results::Create(result)));
}

api::tabs::Tab SessionsGetDevicesFunction::CreateTabModel(
    const std::string& session_tag,
    const sessions::SessionTab& tab,
    int tab_index,
    bool active) {
  std::string session_id = SessionId(session_tag, tab.tab_id.id()).ToString();
  return CreateTabModelHelper(
      tab.navigations[tab.normalized_navigation_index()], session_id, tab_index,
      tab.pinned, active, extension(), source_context_type());
}

std::unique_ptr<api::windows::Window>
SessionsGetDevicesFunction::CreateWindowModel(
    const sessions::SessionWindow& window,
    const std::string& session_tag) {
  DCHECK(!window.tabs.empty());

  // Prune tabs that are not syncable or are NewTabPage. Then, sort the tabs
  // from most recent to least recent.
  std::vector<const sessions::SessionTab*> tabs_in_window;
  for (size_t i = 0; i < window.tabs.size(); ++i) {
    const sessions::SessionTab* tab = window.tabs[i].get();
    if (tab->navigations.empty())
      continue;
    const sessions::SerializedNavigationEntry& current_navigation =
        tab->navigations.at(tab->normalized_navigation_index());
    if (search::IsNTPOrRelatedURL(
            current_navigation.virtual_url(),
            Profile::FromBrowserContext(browser_context()))) {
      continue;
    }
    tabs_in_window.push_back(tab);
  }
  if (tabs_in_window.empty())
    return nullptr;
  std::sort(tabs_in_window.begin(), tabs_in_window.end(), SortTabsByRecency);

  std::unique_ptr<std::vector<api::tabs::Tab>> tabs(
      new std::vector<api::tabs::Tab>());
  for (size_t i = 0; i < tabs_in_window.size(); ++i) {
    tabs->push_back(CreateTabModel(session_tag, *tabs_in_window[i], i,
                                   window.selected_tab_index == (int)i));
  }

  std::string session_id =
      SessionId(session_tag, window.window_id.id()).ToString();

  api::windows::WindowType type = api::windows::WINDOW_TYPE_NONE;
  switch (window.type) {
    case sessions::SessionWindow::TYPE_NORMAL:
      type = api::windows::WINDOW_TYPE_NORMAL;
      break;
    case sessions::SessionWindow::TYPE_POPUP:
      type = api::windows::WINDOW_TYPE_POPUP;
      break;
    case sessions::SessionWindow::TYPE_APP:
      type = api::windows::WINDOW_TYPE_APP;
      break;
    case sessions::SessionWindow::TYPE_DEVTOOLS:
      type = api::windows::WINDOW_TYPE_DEVTOOLS;
      break;
  }

  api::windows::WindowState state = api::windows::WINDOW_STATE_NONE;
  switch (window.show_state) {
    case ui::SHOW_STATE_NORMAL:
      state = api::windows::WINDOW_STATE_NORMAL;
      break;
    case ui::SHOW_STATE_MINIMIZED:
      state = api::windows::WINDOW_STATE_MINIMIZED;
      break;
    case ui::SHOW_STATE_MAXIMIZED:
      state = api::windows::WINDOW_STATE_MAXIMIZED;
      break;
    case ui::SHOW_STATE_FULLSCREEN:
      state = api::windows::WINDOW_STATE_FULLSCREEN;
      break;
    case ui::SHOW_STATE_DEFAULT:
    case ui::SHOW_STATE_INACTIVE:
    case ui::SHOW_STATE_END:
      break;
  }

  std::unique_ptr<api::windows::Window> window_struct(
      CreateWindowModelHelper(std::move(tabs), session_id, type, state));
  // TODO(dwankri): Dig deeper to resolve bounds not being optional, so closed
  // windows in GetRecentlyClosed can have set values in Window helper.
  window_struct->left.reset(new int(window.bounds.x()));
  window_struct->top.reset(new int(window.bounds.y()));
  window_struct->width.reset(new int(window.bounds.width()));
  window_struct->height.reset(new int(window.bounds.height()));

  return window_struct;
}

std::unique_ptr<api::sessions::Session>
SessionsGetDevicesFunction::CreateSessionModel(
    const sessions::SessionWindow& window,
    const std::string& session_tag) {
  std::unique_ptr<api::windows::Window> window_model(
      CreateWindowModel(window, session_tag));
  // There is a chance that after pruning uninteresting tabs the window will be
  // empty.
  return !window_model
             ? nullptr
             : CreateSessionModelHelper(window.timestamp.ToTimeT(),
                                        std::unique_ptr<api::tabs::Tab>(),
                                        std::move(window_model));
}

api::sessions::Device SessionsGetDevicesFunction::CreateDeviceModel(
    const sync_sessions::SyncedSession* session) {
  int max_results = api::sessions::MAX_SESSION_RESULTS;
  // Already validated in RunAsync().
  std::unique_ptr<GetDevices::Params> params(
      GetDevices::Params::Create(*args_));
  if (params->filter && params->filter->max_results)
    max_results = *params->filter->max_results;

  api::sessions::Device device_struct;
  device_struct.info = session->session_name;
  device_struct.device_name = session->session_name;

  for (auto it = session->windows.begin();
       it != session->windows.end() &&
       static_cast<int>(device_struct.sessions.size()) < max_results;
       ++it) {
    std::unique_ptr<api::sessions::Session> session_model(
        CreateSessionModel(it->second->wrapped_window, session->session_tag));
    if (session_model)
      device_struct.sessions.push_back(std::move(*session_model));
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
  std::vector<const sync_sessions::SyncedSession*> sessions;
  // If the user has disabled tab sync, GetOpenTabsUIDelegate() returns null.
  if (!(open_tabs && open_tabs->GetAllForeignSessions(&sessions))) {
    return RespondNow(ArgumentList(
        GetDevices::Results::Create(std::vector<api::sessions::Device>())));
  }

  std::unique_ptr<GetDevices::Params> params(
      GetDevices::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  if (params->filter && params->filter->max_results) {
    EXTENSION_FUNCTION_VALIDATE(*params->filter->max_results >= 0 &&
        *params->filter->max_results <= api::sessions::MAX_SESSION_RESULTS);
  }

  std::vector<api::sessions::Device> result;
  // Sort sessions from most recent to least recent.
  std::sort(sessions.begin(), sessions.end(), SortSessionsByRecency);
  for (size_t i = 0; i < sessions.size(); ++i)
    result.push_back(CreateDeviceModel(sessions[i]));

  return RespondNow(ArgumentList(GetDevices::Results::Create(result)));
}

ExtensionFunction::ResponseValue SessionsRestoreFunction::GetRestoredTabResult(
    content::WebContents* contents) {
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(extension(), source_context_type(),
                                            contents);
  std::unique_ptr<api::tabs::Tab> tab(ExtensionTabUtil::CreateTabObject(
      contents, scrub_tab_behavior, extension()));
  std::unique_ptr<api::sessions::Session> restored_session(
      CreateSessionModelHelper(base::Time::Now().ToTimeT(), std::move(tab),
                               std::unique_ptr<api::windows::Window>()));
  return ArgumentList(Restore::Results::Create(*restored_session));
}

ExtensionFunction::ResponseValue
SessionsRestoreFunction::GetRestoredWindowResult(int window_id) {
  Browser* browser = nullptr;
  std::string error;
  if (!windows_util::GetBrowserFromWindowID(this, window_id, 0, &browser,
                                            &error)) {
    return Error(error);
  }
  std::unique_ptr<base::DictionaryValue> window_value(
      ExtensionTabUtil::CreateWindowValueForExtension(
          *browser, extension(), ExtensionTabUtil::kPopulateTabs,
          source_context_type()));
  std::unique_ptr<api::windows::Window> window(
      api::windows::Window::FromValue(*window_value));
  return ArgumentList(Restore::Results::Create(*CreateSessionModelHelper(
      base::Time::Now().ToTimeT(), std::unique_ptr<api::tabs::Tab>(),
      std::move(window))));
}

ExtensionFunction::ResponseValue
SessionsRestoreFunction::RestoreMostRecentlyClosed(Browser* browser) {
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  const sessions::TabRestoreService::Entries& entries =
      tab_restore_service->entries();

  if (entries.empty())
    return Error(kNoRecentlyClosedSessionsError);

  bool is_window = is_window_entry(*entries.front());
  sessions::LiveTabContext* context =
      BrowserLiveTabContext::FindContextForWebContents(
          browser->tab_strip_model()->GetActiveWebContents());
  std::vector<sessions::LiveTab*> restored_tabs =
      tab_restore_service->RestoreMostRecentEntry(context);
  DCHECK(restored_tabs.size());

  sessions::ContentLiveTab* first_tab =
      static_cast<sessions::ContentLiveTab*>(restored_tabs[0]);
  if (is_window) {
    return GetRestoredWindowResult(
        ExtensionTabUtil::GetWindowIdOfTab(first_tab->web_contents()));
  }

  return GetRestoredTabResult(first_tab->web_contents());
}

ExtensionFunction::ResponseValue SessionsRestoreFunction::RestoreLocalSession(
    const SessionId& session_id,
    Browser* browser) {
  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  const sessions::TabRestoreService::Entries& entries =
      tab_restore_service->entries();

  if (entries.empty())
    return Error(kInvalidSessionIdError, session_id.ToString());

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

  sessions::LiveTabContext* context =
      BrowserLiveTabContext::FindContextForWebContents(
          browser->tab_strip_model()->GetActiveWebContents());
  std::vector<sessions::LiveTab*> restored_tabs =
      tab_restore_service->RestoreEntryById(
          context, SessionID::FromSerializedValue(session_id.id()),
          WindowOpenDisposition::UNKNOWN);
  // If the ID is invalid, restored_tabs will be empty.
  if (restored_tabs.empty())
    return Error(kInvalidSessionIdError, session_id.ToString());

  sessions::ContentLiveTab* first_tab =
      static_cast<sessions::ContentLiveTab*>(restored_tabs[0]);

  // Retrieve the window through any of the tabs in restored_tabs.
  if (is_window) {
    return GetRestoredWindowResult(
        ExtensionTabUtil::GetWindowIdOfTab(first_tab->web_contents()));
  }

  return GetRestoredTabResult(first_tab->web_contents());
}

ExtensionFunction::ResponseValue SessionsRestoreFunction::RestoreForeignSession(
    const SessionId& session_id,
    Browser* browser) {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  sync_sessions::SessionSyncService* service =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  DCHECK(service);

  sync_sessions::OpenTabsUIDelegate* open_tabs =
      service->GetOpenTabsUIDelegate();
  // If the user has disabled tab sync, GetOpenTabsUIDelegate() returns null.
  if (!open_tabs)
    return Error(kSessionSyncError);

  const sessions::SessionTab* tab = NULL;
  if (open_tabs->GetForeignTab(session_id.session_tag(),
                               SessionID::FromSerializedValue(session_id.id()),
                               &tab)) {
    TabStripModel* tab_strip = browser->tab_strip_model();
    content::WebContents* contents = tab_strip->GetActiveWebContents();

    content::WebContents* tab_contents =
        SessionRestore::RestoreForeignSessionTab(
            contents, *tab, WindowOpenDisposition::NEW_FOREGROUND_TAB);
    return GetRestoredTabResult(tab_contents);
  }

  // Restoring a full window.
  std::vector<const sessions::SessionWindow*> windows;
  if (!open_tabs->GetForeignSession(session_id.session_tag(), &windows))
    return Error(kInvalidSessionIdError, session_id.ToString());

  std::vector<const sessions::SessionWindow*>::const_iterator window =
      windows.begin();
  while (window != windows.end()
         && (*window)->window_id.id() != session_id.id()) {
    ++window;
  }
  if (window == windows.end())
    return Error(kInvalidSessionIdError, session_id.ToString());

  // Only restore one window at a time.
  std::vector<Browser*> browsers =
      SessionRestore::RestoreForeignSessionWindows(profile, window, window + 1);
  // Will always create one browser because we only restore one window per call.
  DCHECK_EQ(1u, browsers.size());
  return GetRestoredWindowResult(ExtensionTabUtil::GetWindowId(browsers[0]));
}

ExtensionFunction::ResponseAction SessionsRestoreFunction::Run() {
  std::unique_ptr<Restore::Params> params(Restore::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  Browser* browser = chrome::FindBrowserWithProfile(profile);
  if (!browser)
    return RespondNow(Error(kNoBrowserToRestoreSession));

  if (profile != profile->GetOriginalProfile())
    return RespondNow(Error(kRestoreInIncognitoError));

  if (!params->session_id)
    return RespondNow(RestoreMostRecentlyClosed(browser));

  std::unique_ptr<SessionId> session_id(SessionId::Parse(*params->session_id));
  if (!session_id)
    return RespondNow(Error(kInvalidSessionIdError, *params->session_id));

  return RespondNow(session_id->IsForeign()
                        ? RestoreForeignSession(*session_id, browser)
                        : RestoreLocalSession(*session_id, browser));
}

SessionsEventRouter::SessionsEventRouter(Profile* profile)
    : profile_(profile),
      tab_restore_service_(TabRestoreServiceFactory::GetForProfile(profile)) {
  // TabRestoreServiceFactory::GetForProfile() can return NULL (i.e., when in
  // incognito mode)
  if (tab_restore_service_) {
    tab_restore_service_->LoadTabsFromLastSession();
    tab_restore_service_->AddObserver(this);
  }
}

SessionsEventRouter::~SessionsEventRouter() {
  if (tab_restore_service_)
    tab_restore_service_->RemoveObserver(this);
}

void SessionsEventRouter::TabRestoreServiceChanged(
    sessions::TabRestoreService* service) {
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  EventRouter::Get(profile_)->BroadcastEvent(std::make_unique<Event>(
      events::SESSIONS_ON_CHANGED, api::sessions::OnChanged::kEventName,
      std::move(args)));
}

void SessionsEventRouter::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {
  tab_restore_service_ = NULL;
}

SessionsAPI::SessionsAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter::Get(browser_context_)->RegisterObserver(this,
      api::sessions::OnChanged::kEventName);
}

SessionsAPI::~SessionsAPI() {
}

void SessionsAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<SessionsAPI>>::
    DestructorAtExit g_sessions_api_factory = LAZY_INSTANCE_INITIALIZER;

BrowserContextKeyedAPIFactory<SessionsAPI>*
SessionsAPI::GetFactoryInstance() {
  return g_sessions_api_factory.Pointer();
}

void SessionsAPI::OnListenerAdded(const EventListenerInfo& details) {
  sessions_event_router_.reset(
      new SessionsEventRouter(Profile::FromBrowserContext(browser_context_)));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

}  // namespace extensions
