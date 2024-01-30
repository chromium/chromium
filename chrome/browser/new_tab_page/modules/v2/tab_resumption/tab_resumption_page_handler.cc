// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/tab_resumption/tab_resumption_page_handler.h"

#include <stddef.h>

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/new_tab_page/modules/v2/tab_resumption/tab_resumption.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/tab_resumption/tab_resumption_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/mojom/history_types.mojom.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search/ntp_features.h"
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

std::u16string FormatRelativeTime(const base::Time& time) {
  // Return a time like "1 hour ago", "2 days ago", etc.
  base::Time now = base::Time::Now();
  // TimeFormat does not support negative TimeDelta values, so then we use 0.
  return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                                ui::TimeFormat::LENGTH_SHORT,
                                now < time ? base::TimeDelta() : now - time);
}

// Helper method to create mojom tab objects from SessionTab objects.
history::mojom::TabPtr SessionTabToMojom(const ::sessions::SessionTab& tab,
                                         const std::string& session_tag,
                                         const std::string& session_name) {
  if (tab.navigations.empty()) {
    return nullptr;
  }

  int selected_index = std::min(tab.current_navigation_index,
                                static_cast<int>(tab.navigations.size() - 1));
  const ::sessions::SerializedNavigationEntry& current_navigation =
      tab.navigations.at(selected_index);
  GURL tab_url = current_navigation.virtual_url();
  if (!tab_url.is_valid() || tab_url.spec() == chrome::kChromeUINewTabURL) {
    return nullptr;
  }

  auto tab_mojom = history::mojom::Tab::New();
  tab_mojom->session_tag = session_tag;
  tab_mojom->session_name = session_name;
  base::Value::Dict dictionary;
  NewTabUI::SetUrlTitleAndDirection(&dictionary, current_navigation.title(),
                                    tab_url);
  tab_mojom->url = GURL(*dictionary.FindString("url"));
  tab_mojom->title = *dictionary.FindString("title");

  tab_mojom->relative_time =
      base::UTF16ToUTF8(FormatRelativeTime(tab.timestamp));

  return tab_mojom;
}

// Helper method to append mojom tab objects from SessionWindow objects.
void SessionWindowToMojom(std::vector<history::mojom::TabPtr>& tabs_mojom,
                          const ::sessions::SessionWindow& window,
                          const std::string& session_tag,
                          const std::string& session_name) {
  if (window.tabs.empty()) {
    return;
  }

  for (const std::unique_ptr<sessions::SessionTab>& tab : window.tabs) {
    tabs_mojom.push_back(
        SessionTabToMojom(*tab.get(), session_tag, session_name));
  }
}

// Helper method to create a list of mojom tab objects from Session objects.
std::vector<history::mojom::TabPtr> SessionToMojom(
    const sync_sessions::SyncedSession* session) {
  std::vector<history::mojom::TabPtr> tabs_mojom;
  const std::string& session_tag = session->GetSessionTag();
  const std::string& session_name = session->GetSessionName();

  // Order tabs by visual order within window.
  for (const auto& window_pair : session->windows) {
    SessionWindowToMojom(tabs_mojom, window_pair.second->wrapped_window,
                         session_tag, session_name);
  }
  return tabs_mojom;
}
}  // namespace

TabResumptionPageHandler::TabResumptionPageHandler(
    mojo::PendingReceiver<ntp::tab_resumption::mojom::PageHandler>
        pending_page_handler,
    content::WebContents* web_contents)
    : profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      web_contents_(web_contents),
      page_handler_(this, std::move(pending_page_handler)),
      visibility_threshold_(
          static_cast<float>(base::GetFieldTrialParamByFeatureAsDouble(
              ntp_features::kNtpTabResumptionModule,
              ntp_features::kNtpTabResumptionModuleVisibilityThresholdDataParam,
              /*Default value for visibility threshold*/ 0.5))) {
  DCHECK(profile_);
  DCHECK(web_contents_);
}

TabResumptionPageHandler::~TabResumptionPageHandler() = default;

void TabResumptionPageHandler::OnQueryURLsComplete(
    std::vector<history::mojom::TabPtr> tabs,
    GetTabsCallback callback,
    std::vector<history::QueryURLResult> results) {
  history::VisitVector visit_rows;
  for (auto result : results) {
    for (auto visit : result.visits) {
      visit_rows.push_back(visit);
    }
  }
  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  history_service->ToAnnotatedVisits(
      visit_rows,
      /*compute_redirect_chain_start_properties=*/false,
      base::BindOnce(&TabResumptionPageHandler::OnAnnotatedVisits,
                     weak_ptr_factory_.GetWeakPtr(), std::move(tabs),
                     std::move(callback)),
      &task_tracker_);
}

void TabResumptionPageHandler::OnAnnotatedVisits(
    std::vector<history::mojom::TabPtr> tabs,
    GetTabsCallback callback,
    const std::vector<history::AnnotatedVisit> annotated_visits) {
  std::vector<history::mojom::TabPtr> scored_tabs;
  std::set<int> scored_tab_indices;
  for (const auto& annotated_visit : annotated_visits) {
    float visibility_score =
        annotated_visit.content_annotations.model_annotations.visibility_score;
    /* If score is -1, it has not been evaluated for visibility */
    if (visibility_score < visibility_threshold_ && visibility_score >= 0) {
      continue;
    }
    for (size_t i = 0; i < tabs.size(); i++) {
      if (annotated_visit.url_row.url() == tabs[i]->url &&
          scored_tab_indices.find(i) == scored_tab_indices.end()) {
        scored_tab_indices.insert(i);
        break;
      }
    }
  }

  for (auto i : scored_tab_indices) {
    scored_tabs.push_back(std::move(tabs[i]));
  }

  std::move(callback).Run(std::move(scored_tabs));
}

void TabResumptionPageHandler::GetTabs(GetTabsCallback callback) {
  const std::string fake_data_param = base::GetFieldTrialParamValueByFeature(
      ntp_features::kNtpTabResumptionModule,
      ntp_features::kNtpTabResumptionModuleDataParam);

  if (!fake_data_param.empty()) {
    std::vector<history::mojom::TabPtr> tabs_mojom;
    const int kSampleSessionsCount = 3;
    for (int i = 0; i < kSampleSessionsCount; i++) {
      auto session_tabs_mojom = SessionToMojom(
          SampleSession("Test Name",
                        ("Test Tag " + base::NumberToString(i)).c_str(), 3, 1)
              .get());
      for (auto& tab_mojom : session_tabs_mojom) {
        tabs_mojom.push_back(std::move(tab_mojom));
      }
    }
    std::move(callback).Run(std::move(tabs_mojom));
    return;
  }

  auto tabs_mojom = GetForeignTabs();
  std::vector<GURL> urls;
  for (const auto& tab : tabs_mojom) {
    urls.push_back(tab->url);
  }
  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  history_service->QueryURLs(
      urls, /*want_visits=*/true,
      base::BindOnce(&TabResumptionPageHandler::OnQueryURLsComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(tabs_mojom),
                     std::move(callback)),
      &task_tracker_);
}

// static
sync_sessions::OpenTabsUIDelegate*
TabResumptionPageHandler::GetOpenTabsUIDelegate() {
  sync_sessions::SessionSyncService* service =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  return service ? service->GetOpenTabsUIDelegate() : nullptr;
}

std::vector<history::mojom::TabPtr> TabResumptionPageHandler::GetForeignTabs() {
  sync_sessions::OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate();
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      sessions;

  std::vector<history::mojom::TabPtr> tabs_mojom;
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
      const sync_sessions::SyncedSession* session(sessions[i]);
      auto session_tabs_mojom = SessionToMojom(session);
      const std::string& session_tag = session->GetSessionTag();
      bool is_collapsed = collapsed_sessions.Find(session_tag);
      if (is_collapsed) {
        current_collapsed_sessions.Set(session_tag, true);
      }
      for (auto& tab_mojom : session_tabs_mojom) {
        tabs_mojom.push_back(std::move(tab_mojom));
      }
    }
  }
  return tabs_mojom;
}
