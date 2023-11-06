// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/tab_resumption/tab_resumption_page_handler.h"

#include <stddef.h>

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/new_tab_page/modules/v2/tab_resumption/tab_resumption.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/history/core/browser/mojom/history_types.mojom.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "url/gurl.h"

using history::BrowsingHistoryService;
using history::HistoryService;

namespace {
// Maximum number of sessions we're going to display on the NTP
const size_t kMaxSessionsToShow = 10;

// Helper method to create mojom session objects from Session objects.
absl::optional<history::mojom::TabPtr> SessionTabToMojom(
    const ::sessions::SessionTab& tab) {
  if (tab.navigations.empty()) {
    return absl::nullopt;
  }

  int selected_index = std::min(tab.current_navigation_index,
                                static_cast<int>(tab.navigations.size() - 1));
  const ::sessions::SerializedNavigationEntry& current_navigation =
      tab.navigations.at(selected_index);
  GURL tab_url = current_navigation.virtual_url();
  if (!tab_url.is_valid() || tab_url.spec() == chrome::kChromeUINewTabURL) {
    return absl::nullopt;
  }

  auto tab_mojom = history::mojom::Tab::New();
  base::Value::Dict dictionary;
  NewTabUI::SetUrlTitleAndDirection(&dictionary, current_navigation.title(),
                                    tab_url);
  tab_mojom->url = GURL(*dictionary.FindString("url"));
  tab_mojom->title = *dictionary.FindString("title");

  tab_mojom->timestamp = tab.timestamp;
  // Seen also: http://crbug.com/154865.
  // Renamed to window id to match definitions in
  // chrome/browser/resources/history/externs.ts.
  tab_mojom->window_id = tab.tab_id.id();
  return tab_mojom;
}

// Helper for initializing a boilerplate SessionWindow Mojom object.
history::mojom::WindowPtr BuildWindowMojom(base::Time modification_time,
                                           SessionID window_id) {
  auto window_mojom = history::mojom::Window::New();
  window_mojom->timestamp = modification_time;

  window_mojom->session_id = window_id.id();
  return window_mojom;
}

// Helper method to create mojom window objects from SessionWindow objects.
absl::optional<history::mojom::WindowPtr> SessionWindowToMojom(
    const ::sessions::SessionWindow& window) {
  if (window.tabs.empty()) {
    return absl::nullopt;
  }

  std::vector<absl::optional<history::mojom::TabPtr>> tabs_mojom;
  auto window_mojom = BuildWindowMojom(window.timestamp, window.window_id);
  for (const std::unique_ptr<sessions::SessionTab>& tab : window.tabs) {
    auto tab_mojom = SessionTabToMojom(*tab.get());
    if (tab_mojom) {
      window_mojom->tabs.push_back(std::move(*tab_mojom));
    }
  }
  if (window_mojom->tabs.empty()) {
    return absl::nullopt;
  }

  return window_mojom;
}
}  // namespace

TabResumptionPageHandler::TabResumptionPageHandler(
    mojo::PendingReceiver<ntp::tab_resumption::mojom::PageHandler>
        pending_page_handler,
    Profile* profile,
    content::WebContents* web_contents)
    : profile_(profile),
      web_contents_(web_contents),
      page_handler_(this, std::move(pending_page_handler)) {
  DCHECK(profile_);
  DCHECK(web_contents_);
}

TabResumptionPageHandler::~TabResumptionPageHandler() = default;

void TabResumptionPageHandler::GetTabs(GetTabsCallback callback) {
  auto sessions_mojom = GetForeignSessions();
  std::move(callback).Run(std::move(sessions_mojom));
}

// static
sync_sessions::OpenTabsUIDelegate*
TabResumptionPageHandler::GetOpenTabsUIDelegate() {
  sync_sessions::SessionSyncService* service =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  return service ? service->GetOpenTabsUIDelegate() : nullptr;
}

std::vector<history::mojom::SessionPtr>
TabResumptionPageHandler::GetForeignSessions() {
  sync_sessions::OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate();
  std::vector<const sync_sessions::SyncedSession*> sessions;

  std::vector<history::mojom::SessionPtr> sessions_mojom;
  if (open_tabs && open_tabs->GetAllForeignSessions(&sessions)) {
    // Use a pref to keep track of sessions that were collapsed by the user.
    // To prevent the pref from accumulating stale sessions, clear it each time
    // and only add back sessions that are still current.
    ScopedDictPrefUpdate pref_update(profile_->GetPrefs(),
                                     prefs::kNtpCollapsedForeignSessions);
    base::Value::Dict& current_collapsed_sessions = pref_update.Get();
    base::Value::Dict collapsed_sessions = current_collapsed_sessions.Clone();
    current_collapsed_sessions.clear();

    // Note: we don't own the SyncedSessions themselves.
    for (size_t i = 0; i < sessions.size() && i < kMaxSessionsToShow; ++i) {
      const sync_sessions::SyncedSession* session = sessions[i];
      const std::string& session_tag = session->GetSessionTag();
      auto session_mojom = history::mojom::Session::New();
      session_mojom->tag = session_tag;
      session_mojom->name = session->GetSessionName();
      base::Time now = base::Time::Now();
      base::Time time = session->GetModifiedTime();
      session_mojom->modified_time =
          now < time ? base::TimeDelta() : now - time;
      session_mojom->timestamp = session->GetModifiedTime();

      bool is_collapsed = collapsed_sessions.Find(session_tag);
      session_mojom->collapsed = is_collapsed;
      if (is_collapsed) {
        current_collapsed_sessions.Set(session_tag, true);
      }

      // Order tabs by visual order within window.
      for (const auto& window_pair : session->windows) {
        auto window = SessionWindowToMojom(window_pair.second->wrapped_window);
        if (window) {
          session_mojom->windows.push_back(std::move(*window));
        }
      }

      sessions_mojom.push_back(std::move(session_mojom));
    }
  }
  return sessions_mojom;
}
