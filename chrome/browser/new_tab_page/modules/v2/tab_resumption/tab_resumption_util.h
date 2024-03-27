// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_TAB_RESUMPTION_TAB_RESUMPTION_UTIL_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_TAB_RESUMPTION_TAB_RESUMPTION_UTIL_H_

#include <memory>

#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/mojom/history_types.mojom.h"
#include "components/sync_sessions/synced_session.h"

inline constexpr char kSampleUrl[] = "https://www.google.com";

std::unique_ptr<sync_sessions::SyncedSession> SampleSession(
    const char session_name[],
    int num_windows,
    int num_tabs,
    std::vector<base::Time>& timestamps);

std::unique_ptr<sync_sessions::SyncedSession>
SampleSession(const char session_name[], int num_windows, int num_tabs);

std::vector<std::unique_ptr<sync_sessions::SyncedSession>> SampleSessions(
    int num_sessions,
    int num_tabs,
    std::vector<base::Time> timestamps);

base::flat_set<std::string> GetTabResumptionCategories(
    const char* feature_param,
    base::span<const std::string_view> default_categories);

bool IsVisitInCategories(const history::AnnotatedVisit& annotated_visit,
                         const base::flat_set<std::string>& categories);

bool CompareTabsByTime(history::mojom::TabPtr& tab1,
                       history::mojom::TabPtr& tab2);

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_TAB_RESUMPTION_TAB_RESUMPTION_UTIL_H_
