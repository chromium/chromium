// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_HISTORY_MOCK_PRINT_JOB_HISTORY_SERVICE_H_
#define CHROME_BROWSER_ASH_PRINTING_HISTORY_MOCK_PRINT_JOB_HISTORY_SERVICE_H_

#include "base/functional/callback.h"
#include "chrome/browser/ash/printing/history/print_job_history_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

// The mock implementation of PrintJobHistoryService for testing.
class MockPrintJobHistoryService : public PrintJobHistoryService {
 public:
  MockPrintJobHistoryService();
  ~MockPrintJobHistoryService() override;

  // This method doesn't save print job to the persistent storage.
  // It should be used only for testing to notify observers.
  void SavePrintJobProto(const printing::proto::PrintJobInfo& print_job_info);

  MOCK_METHOD(void,
              GetPrintJobs,
              (PrintJobDatabase::GetPrintJobsCallback callback),
              (override));

  MOCK_METHOD(void,
              DeleteAllPrintJobs,
              (PrintJobDatabase::DeletePrintJobsCallback callback),
              (override));
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_HISTORY_MOCK_PRINT_JOB_HISTORY_SERVICE_H_
