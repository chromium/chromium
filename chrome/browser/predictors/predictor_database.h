// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_PREDICTOR_DATABASE_H_
#define CHROME_BROWSER_PREDICTORS_PREDICTOR_DATABASE_H_

#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace base {
class SequencedTaskRunner;
}

namespace sql {
class Database;
}

namespace predictors {

class AutocompleteActionPredictorTable;
class PredictorDatabaseInternal;
class ResourcePrefetchPredictorTables;

class PredictorDatabase : public KeyedService {
 public:
  PredictorDatabase(Profile* profile,
                    scoped_refptr<base::SequencedTaskRunner> db_task_runner);

  PredictorDatabase(const PredictorDatabase&) = delete;
  PredictorDatabase& operator=(const PredictorDatabase&) = delete;

  ~PredictorDatabase() override;

  scoped_refptr<AutocompleteActionPredictorTable> autocomplete_table();
  scoped_refptr<ResourcePrefetchPredictorTables> resource_prefetch_tables();

  // Used for testing.
  sql::Database* GetDatabase();

 private:
  // KeyedService
  void Shutdown() override;

  scoped_refptr<PredictorDatabaseInternal> db_;
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_PREDICTOR_DATABASE_H_
