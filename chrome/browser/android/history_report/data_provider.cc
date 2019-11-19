// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history_report/data_provider.h"

#include <stddef.h>

#include <map>

#include "base/bind.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "chrome/browser/android/history_report/delta_file_commons.h"
#include "chrome/browser/android/history_report/delta_file_service.h"
#include "chrome/browser/android/history_report/get_all_urls_from_history_task.h"
#include "chrome/browser/android/history_report/historic_visits_migration_task.h"
#include "chrome/browser/android/history_report/usage_reports_buffer_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/model_loader.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using bookmarks::BookmarkModel;
using bookmarks::UrlAndTitle;

namespace {
static bool g_is_debug = false;

using BookmarkMap = std::map<std::string, UrlAndTitle*>;

struct Context {
  history::HistoryService* history_service;
  base::CancelableTaskTracker* history_task_tracker;
  base::WaitableEvent finished;

  Context(history::HistoryService* hservice,
          base::CancelableTaskTracker* tracker)
      : history_service(hservice),
        history_task_tracker(tracker),
        finished(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                 base::WaitableEvent::InitialState::NOT_SIGNALED) {}
};

void UpdateUrl(Context* context,
               size_t position,
               std::vector<history_report::DeltaFileEntryWithData>* urls,
               history::QueryURLResult result) {
  history_report::DeltaFileEntryWithData* entry = &((*urls)[position]);
  if (result.success) {
    entry->SetData(result.row);
  } else if (g_is_debug) {
    LOG(WARNING) << "DB not initialized or no data for url " << entry->Url();
  }
  if (position + 1 == urls->size()) {
    context->finished.Signal();
  } else {
    context->history_service->QueryURL(
        GURL((*urls)[position + 1].Url()), false,
        base::BindOnce(&UpdateUrl, base::Unretained(context), position + 1,
                       base::Unretained(urls)),
        context->history_task_tracker);
  }
}

void QueryUrlsHistoryInUiThread(
    Context* context,
    std::vector<history_report::DeltaFileEntryWithData>* urls) {
  context->history_task_tracker->TryCancelAll();
  // TODO(haaawk): change history service so that all this data can be
  //               obtained with a single call to history service.
  context->history_service->QueryURL(
      GURL((*urls)[0].Url()), false,
      base::BindOnce(&UpdateUrl, base::Unretained(context), 0,
                     base::Unretained(urls)),
      context->history_task_tracker);
}

void StartVisitMigrationToUsageBufferUiThread(
    history::HistoryService* history_service,
    history_report::UsageReportsBufferService* buffer_service,
    base::WaitableEvent* finished,
    base::CancelableTaskTracker* task_tracker) {
  history_service->ScheduleDBTask(
      FROM_HERE,
      std::unique_ptr<history::HistoryDBTask>(
          new history_report::HistoricVisitsMigrationTask(finished,
                                                          buffer_service)),
      task_tracker);
}

}  // namespace

namespace history_report {

DataProvider::DataProvider(Profile* profile,
                           DeltaFileService* delta_file_service,
                           BookmarkModel* bookmark_model)
    : bookmark_model_(bookmark_model),
      delta_file_service_(delta_file_service) {
  history_service_ = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
}

DataProvider::~DataProvider() {}

std::unique_ptr<std::vector<DeltaFileEntryWithData>> DataProvider::Query(
    int64_t last_seq_no,
    int32_t limit) {
  if (last_seq_no == 0)
    RecreateLog();
  std::unique_ptr<std::vector<DeltaFileEntryWithData>> entries;
  std::unique_ptr<std::vector<DeltaFileEntryWithData>> valid_entries;
  do {
    entries = delta_file_service_->Query(last_seq_no, limit);
    if (!entries->empty()) {
      Context context(history_service_,
                      &history_task_tracker_);
      base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                     base::BindOnce(&QueryUrlsHistoryInUiThread,
                                    base::Unretained(&context),
                                    base::Unretained(entries.get())));
      std::vector<UrlAndTitle> bookmarks;
      bookmark_model_->model_loader()->BlockTillLoaded();
      bookmark_model_->GetBookmarks(&bookmarks);
      BookmarkMap bookmark_map;
      for (size_t i = 0; i < bookmarks.size(); ++i) {
        bookmark_map.insert(
            make_pair(bookmarks[i].url.spec(), &bookmarks[i]));
      }
      context.finished.Wait();
      for (size_t i = 0; i < entries->size(); ++i) {
        BookmarkMap::iterator bookmark =
            bookmark_map.find((*entries)[i].Url());
        if (bookmark != bookmark_map.end())
          (*entries)[i].MarkAsBookmark(*(bookmark->second));
      }
    }

    valid_entries.reset(new std::vector<DeltaFileEntryWithData>());
    valid_entries->reserve(entries->size());
    for (size_t i = 0; i < entries->size(); ++i) {
      const DeltaFileEntryWithData& entry = (*entries)[i];
      if (entry.Valid()) valid_entries->push_back(entry);
      if (entry.SeqNo() > last_seq_no) last_seq_no = entry.SeqNo();
    }
  } while (!entries->empty() && valid_entries->empty());
  return valid_entries;
}

void DataProvider::StartVisitMigrationToUsageBuffer(
    UsageReportsBufferService* buffer_service) {
  base::WaitableEvent finished(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
  buffer_service->Clear();
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&StartVisitMigrationToUsageBufferUiThread,
                                base::Unretained(history_service_),
                                buffer_service, base::Unretained(&finished),
                                base::Unretained(&history_task_tracker_)));
  finished.Wait();
}

void DataProvider::RecreateLog() {
  std::vector<std::string> urls;
  {
    base::WaitableEvent finished(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);

    std::unique_ptr<history::HistoryDBTask> task =
        std::unique_ptr<history::HistoryDBTask>(
            new GetAllUrlsFromHistoryTask(&finished, &urls));
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(
            base::IgnoreResult(&history::HistoryService::ScheduleDBTask),
            base::Unretained(history_service_), FROM_HERE, std::move(task),
            base::Unretained(&history_task_tracker_)));
    finished.Wait();
  }

  std::vector<UrlAndTitle> bookmarks;
  bookmark_model_->model_loader()->BlockTillLoaded();
  bookmark_model_->GetBookmarks(&bookmarks);
  urls.reserve(urls.size() + bookmarks.size());
  for (size_t i = 0; i < bookmarks.size(); i++) {
    urls.push_back(bookmarks[i].url.spec());
  }
  delta_file_service_->Recreate(urls);
}

}  // namespace history_report
