// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history_report/historic_visits_migration_task.h"

#include "chrome/browser/android/history_report/delta_file_commons.h"
#include "chrome/browser/android/history_report/usage_report_util.h"
#include "chrome/browser/android/history_report/usage_reports_buffer_service.h"
#include "components/history/core/browser/android/android_history_types.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_database.h"

namespace {
// The number of recent visits to fetch for a typed url.
// Older visits are ignored.
static const int kMaxTypedUrlVisits = 1000;
}

namespace history_report {

HistoricVisitsMigrationTask::HistoricVisitsMigrationTask(
    base::WaitableEvent* event,
    UsageReportsBufferService* report_buffer_service)
    : wait_event_(event), usage_reports_buffer_service_(report_buffer_service) {
}

bool HistoricVisitsMigrationTask::RunOnDBThread(
    history::HistoryBackend* backend,
    history::HistoryDatabase* db) {
  history::URLRows typed_urls;
  backend->GetAllTypedURLs(&typed_urls);

  for (history::URLRows::const_iterator typed_url = typed_urls.begin();
       typed_url != typed_urls.end();
       ++typed_url) {
    std::string url_id =
        DeltaFileEntryWithData::UrlToId(typed_url->url().spec());
    history::VisitVector url_visits;

    if (usage_report_util::ShouldIgnoreUrl(typed_url->url())) {
      continue;
    }
    backend->GetMostRecentVisitsForURL(
        typed_url->id(), kMaxTypedUrlVisits, &url_visits);

    for (history::VisitVector::const_iterator visit = url_visits.begin();
         visit != url_visits.end();
         ++visit) {
      usage_reports_buffer_service_->AddVisit(
          url_id, visit->visit_time.InMillisecondsSinceUnixEpoch(),
          usage_report_util::IsTypedVisit(visit->transition));
    }
  }
  wait_event_->Signal();
  return true;
}

}  // namespace history_report
