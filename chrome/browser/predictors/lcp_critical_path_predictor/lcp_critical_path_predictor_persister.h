// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_PERSISTER_H_
#define CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_PERSISTER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor.pb.h"
#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_database.h"
#include "components/sqlite_proto/key_value_data.h"
#include "sql/database.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

// LCPCriticalPathPredictorPersister implements the service-specific
// persistent logic using LCPCriticalPathPredictorDatabase as a backend
// database.
class LCPCriticalPathPredictorPersister {
 public:
  // Constructs a LCPCriticalPathPredictorPersister backed by |database|.
  explicit LCPCriticalPathPredictorPersister(
      std::unique_ptr<LCPCriticalPathPredictorDatabase> database);
  ~LCPCriticalPathPredictorPersister();

  // Constructs a LCPCriticalPathPredictorPersister backed by an on-disk
  // database:
  // - `db_task_runner` will be used for posting blocking database IO;
  // - `path` will store the database; The path must be within the user profile
  // directory (except for tests).
  // - `flush_delay_for_writes` is the maximum time before each write is flushed
  // to the underlying database.
  //
  // `on_done_initializing` will be called once the persister's underlying
  // state has been initialized from disk.
  //
  // If initialization fails, `on_done_initializing` will still be provided a
  // non-null pointer to a usable LCPCriticalPathPredictorPersister, but the
  // persister will only cache writes to memory, rather than persist them to
  // disk.
  static void CreateForFilePath(
      scoped_refptr<base::SequencedTaskRunner> db_task_runner,
      const base::FilePath& path,
      base::TimeDelta flush_delay_for_writes,
      base::OnceCallback<
          void(std::unique_ptr<LCPCriticalPathPredictorPersister>)>
          on_done_initializing);

  absl::optional<LCPElement> GetLCPElement(const GURL& page_url);

  void SetLCPElement(const GURL& page_url, const LCPElement& lcp_element);
  // TODO(crbug.com/1419756): Support the removal data from the database. e.g.
  // Observe history deletion events and delete data for history entries that
  // have been deleted.

 private:
  // Manages the underlying database.
  std::unique_ptr<LCPCriticalPathPredictorDatabase> database_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_PERSISTER_H_
