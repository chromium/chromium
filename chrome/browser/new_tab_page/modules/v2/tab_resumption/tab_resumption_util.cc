// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/tab_resumption/tab_resumption_util.h"

#include <memory>
#include <string>

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/mojom/history_types.mojom.h"
#include "components/search/ntp_features.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/synced_session.h"

namespace {

std::unique_ptr<sessions::SessionTab> SampleSessionTab(
    int tab_id,
    base::Time timestamp = base::Time::Now()) {
  auto session_tab = std::make_unique<sessions::SessionTab>();
  session_tab->current_navigation_index = 0;

  sessions::SerializedNavigationEntry navigation;
  navigation.set_title(u"Test");
  size_t url_suffix = (base::Time::Now() - timestamp).InMinutes();
  navigation.set_virtual_url(GURL(
      kSampleUrl + (url_suffix != 0 ? base::NumberToString(url_suffix) : "")));
  navigation.set_timestamp(base::Time::Now());
  navigation.set_favicon_url(GURL(kSampleUrl));
  session_tab->navigations.push_back(navigation);

  session_tab->timestamp = timestamp;
  session_tab->tab_id = SessionID::FromSerializedValue(tab_id);

  return session_tab;
}

std::unique_ptr<sync_sessions::SyncedSessionWindow> SampleSessionWindow(
    int num_tabs,
    std::vector<base::Time>& timestamps) {
  auto synced_session_window =
      std::make_unique<sync_sessions::SyncedSessionWindow>();
  synced_session_window->wrapped_window.timestamp = base::Time::Now();
  for (int i = 0; i < num_tabs; i++) {
    synced_session_window->wrapped_window.tabs.push_back(SampleSessionTab(
        i, timestamps.empty() ? base::Time::Now() : timestamps.back()));
    if (!timestamps.empty()) {
      timestamps.pop_back();
    }
  }
  return synced_session_window;
}

}  // namespace

std::unique_ptr<sync_sessions::SyncedSession> SampleSession(
    const char session_name[],
    int num_windows,
    int num_tabs,
    std::vector<base::Time>& timestamps) {
  auto sample_session = std::make_unique<sync_sessions::SyncedSession>();
  for (int i = 0; i < num_windows; i++) {
    sample_session->windows[SessionID::FromSerializedValue(i)] =
        SampleSessionWindow(num_tabs, timestamps);
  }

  sample_session->SetSessionName(session_name);
  sample_session->SetModifiedTime(base::Time::Now());

  return sample_session;
}

std::unique_ptr<sync_sessions::SyncedSession>
SampleSession(const char session_name[], int num_windows, int num_tabs) {
  auto sample_session = std::make_unique<sync_sessions::SyncedSession>();
  std::vector<base::Time> timestamps = {};
  for (int i = 0; i < num_windows; i++) {
    sample_session->windows[SessionID::FromSerializedValue(i)] =
        SampleSessionWindow(num_tabs, timestamps);
  }

  sample_session->SetSessionName(session_name);
  sample_session->SetModifiedTime(base::Time::Now());

  return sample_session;
}

std::vector<std::unique_ptr<sync_sessions::SyncedSession>> SampleSessions(
    int num_sessions,
    int num_tabs,
    std::vector<base::Time> timestamps) {
  std::vector<std::unique_ptr<sync_sessions::SyncedSession>> sample_sessions;
  for (int i = 0; i < num_sessions; i++) {
    sample_sessions.push_back(
        SampleSession(("Test Name " + base::NumberToString(i)).c_str(), 1,
                      num_tabs, timestamps));
  }
  return sample_sessions;
}

base::flat_set<std::string> GetTabResumptionCategories(
    const char* feature_param,
    base::span<const std::string_view> default_categories) {
  std::string categories_string = base::GetFieldTrialParamValueByFeature(
      ntp_features::kNtpTabResumptionModuleCategories, feature_param);
  if (categories_string.empty()) {
    return base::flat_set<std::string>(default_categories.begin(),
                                       default_categories.end());
  }

  auto categories = base::SplitString(categories_string, ",",
                                      base::WhitespaceHandling::TRIM_WHITESPACE,
                                      base::SplitResult::SPLIT_WANT_NONEMPTY);

  return categories.empty() ? base::flat_set<std::string>()
                            : base::flat_set<std::string>(categories.begin(),
                                                          categories.end());
}

bool IsVisitInCategories(const history::AnnotatedVisit& annotated_visit,
                         const base::flat_set<std::string>& categories) {
  for (const auto& visit_category :
       annotated_visit.content_annotations.model_annotations.categories) {
    if (categories.contains(visit_category.id)) {
      return true;
    }
  }
  return false;
}

bool CompareTabsByTime(history::mojom::TabPtr& tab1,
                       history::mojom::TabPtr& tab2) {
  return tab1->relative_time < tab2->relative_time;
}
