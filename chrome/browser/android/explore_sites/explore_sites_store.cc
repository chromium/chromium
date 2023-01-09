// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_store.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/android/explore_sites/explore_sites_schema.h"
#include "components/offline_pages/core/offline_clock.h"
#include "sql/database.h"

namespace explore_sites {
namespace {
using offline_pages::SqlStoreBase;

const char kExploreSitesStoreFileName[] = "ExploreSitesStore.db";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ExploreSitesStoreEvent {
  kReopened = 0,
  kOpenedFirstTime = 1,
  kCloseSkipped = 2,
  kClosed = 3,
  kMaxValue = kClosed,
};

void ReportStoreEvent(ExploreSitesStoreEvent event) {
  UMA_HISTOGRAM_ENUMERATION("ExploreSites.ExploreSitesStore.StoreEvent", event);
}

}  // namespace

ExploreSitesStore::ExploreSitesStore(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : SqlStoreBase("ExploreSitesStore",
                   std::move(blocking_task_runner),
                   base::FilePath()) {}

ExploreSitesStore::ExploreSitesStore(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const base::FilePath& path)
    : SqlStoreBase("ExploreSitesStore",
                   std::move(blocking_task_runner),
                   path.AppendASCII(kExploreSitesStoreFileName)) {}

ExploreSitesStore::~ExploreSitesStore() {}

base::OnceCallback<bool(sql::Database* db)>
ExploreSitesStore::GetSchemaInitializationFunction() {
  return base::BindOnce(&ExploreSitesSchema::CreateOrUpgradeIfNeeded);
}

void ExploreSitesStore::OnOpenStart(base::TimeTicks last_closing_time) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("explore_sites", "ExploreSitesStore", this,
                                    "is reopen", !last_closing_time.is_null());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("explore_sites", "Initializing", this);
  if (!last_closing_time.is_null()) {
    ReportStoreEvent(ExploreSitesStoreEvent::kReopened);
  } else {
    ReportStoreEvent(ExploreSitesStoreEvent::kOpenedFirstTime);
  }
}

void ExploreSitesStore::OnOpenDone(bool success) {
  TRACE_EVENT_NESTABLE_ASYNC_END1("explore_sites", "Initializing", this,
                                  "succeeded", success);
  if (!success) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("explore_sites", "ExploreSitesStore", this);
  } else {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("explore_sites", "Open", this);
  }
}

void ExploreSitesStore::OnTaskBegin(bool is_initialized) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("explore_sites", "Task execution", this,
                                    "is store loaded", is_initialized);
}

void ExploreSitesStore::OnTaskRunComplete() {
  // Note: the time recorded for this trace step will include thread hop wait
  // times to the background thread and back.
  TRACE_EVENT_NESTABLE_ASYNC_END0("explore_sites", "Task", this);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("explore_sites", "Callback", this);
}

void ExploreSitesStore::OnTaskReturnComplete() {
  TRACE_EVENT_NESTABLE_ASYNC_END0("explore_sites", "Callback", this);
  TRACE_EVENT_NESTABLE_ASYNC_END0("explore_sites", "Task execution", this);
}

void ExploreSitesStore::OnCloseStart(
    InitializationStatus initialization_status) {
  if (initialization_status != InitializationStatus::kSuccess) {
    ReportStoreEvent(ExploreSitesStoreEvent::kCloseSkipped);
    return;
  }
  TRACE_EVENT_NESTABLE_ASYNC_END0("explore_sites", "Open", this);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("explore_sites", "Closing", this);

  ReportStoreEvent(ExploreSitesStoreEvent::kClosed);
}

void ExploreSitesStore::OnCloseComplete() {
  TRACE_EVENT_NESTABLE_ASYNC_END0("explore_sites", "Closing", this);
  TRACE_EVENT_NESTABLE_ASYNC_END0("explore_sites", "ExploreSitesStore", this);
}

}  // namespace explore_sites
