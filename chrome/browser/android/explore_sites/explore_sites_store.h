// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_STORE_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_STORE_H_

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "components/offline_pages/task/sql_store_base.h"

namespace sql {
class Database;
}

namespace explore_sites {

// ExploreSitesStore is a front end to SQLite store hosting the explore sites
// web catalog.
//
// The store controls the pointer to the SQLite database and only makes it
// available to the |RunCallback| of the |Execute| method on the blocking
// thread.
class ExploreSitesStore : public offline_pages::SqlStoreBase {
 public:
  // Creates an instance of |ExploreSitesStore| with an in-memory SQLite
  // database.
  explicit ExploreSitesStore(
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

  // Creates an instance of |ExploreSitesStore| with a SQLite database stored in
  // |database_dir|.
  ExploreSitesStore(
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      const base::FilePath& database_dir);

  ~ExploreSitesStore() override;

 protected:
  // SqlStoreBase:
  base::OnceCallback<bool(sql::Database* db)> GetSchemaInitializationFunction()
      override;
  void OnOpenStart(base::TimeTicks last_open_time) override;
  void OnOpenDone(bool success) override;
  void OnTaskBegin(bool is_initialized) override;
  void OnTaskRunComplete() override;
  void OnTaskReturnComplete() override;
  void OnCloseStart(InitializationStatus status_before_close) override;
  void OnCloseComplete() override;
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_STORE_H_
