// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_PREDICTOR_TABLE_BASE_H_
#define CHROME_BROWSER_PREDICTORS_PREDICTOR_TABLE_BASE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/atomic_flag.h"

namespace base {
class SequencedTaskRunner;
}

namespace sql {
class Database;
}

namespace predictors {

// Base class for all tables in the PredictorDatabase.
//
// Refcounted as it is created and destroyed in the UI thread but all database
// related functions need to happen in the database sequence. The task runner
// for this sequence is provided by the client to the constructor of this class.
class PredictorTableBase
    : public base::RefCountedThreadSafe<PredictorTableBase> {
 public:
  // Returns a SequencedTaskRunner that is used to run tasks on the DB sequence.
  base::SequencedTaskRunner* GetTaskRunner();

 protected:
  explicit PredictorTableBase(
      scoped_refptr<base::SequencedTaskRunner> db_task_runner);
  virtual ~PredictorTableBase();

  // DB sequence functions.
  virtual void CreateTableIfNonExistent() = 0;
  virtual void LogDatabaseStats() = 0;
  void Initialize(sql::Database* db);
  void SetCancelled();
  bool IsCancelled();
  sql::Database* DB();
  void ResetDB();

  bool CantAccessDatabase();

 private:
  base::AtomicFlag cancelled_;

  friend class base::RefCountedThreadSafe<PredictorTableBase>;

  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;
  sql::Database* db_;

  DISALLOW_COPY_AND_ASSIGN(PredictorTableBase);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_PREDICTOR_TABLE_BASE_H_
