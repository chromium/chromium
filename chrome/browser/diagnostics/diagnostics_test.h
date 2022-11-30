// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIAGNOSTICS_DIAGNOSTICS_TEST_H_
#define CHROME_BROWSER_DIAGNOSTICS_DIAGNOSTICS_TEST_H_

#include <stddef.h>

#include "base/time/time.h"
#include "chrome/browser/diagnostics/diagnostics_metrics.h"
#include "chrome/browser/diagnostics/diagnostics_model.h"

namespace base {
class FilePath;
}

namespace diagnostics {

// Represents a single diagnostic test and encapsulates the common
// functionality across platforms as well.
// It also implements the TestInfo interface providing the storage
// for the outcome of the test.
// Specific tests need (minimally) only to:
// 1- override ExecuteImpl() to implement the test.
// 2- call RecordStopFailure() or RecordFailure() or RecordSuccess()
//    at the end of the test.
// 3- Optionally call observer->OnProgress() if the test is long.
// 4- Optionally call observer->OnSkipped() if the test cannot be run.
class DiagnosticsTest : public DiagnosticsModel::TestInfo {
 public:
  explicit DiagnosticsTest(DiagnosticsTestId id);

  ~DiagnosticsTest() override;

  // Runs the test. Returning false signals that no more tests should be run.
  // The actual outcome of the test should be set using the RecordXX functions.
  bool Execute(DiagnosticsModel::Observer* observer, DiagnosticsModel* model,
               size_t index);

  // Runs any recovery steps for the test. Returning false signals that no more
  // recovery should be attempted.
  bool Recover(DiagnosticsModel::Observer* observer, DiagnosticsModel* model,
               size_t index);

  void RecordStopFailure(int outcome_code, const std::string& additional_info) {
    RecordOutcome(
        outcome_code, additional_info, DiagnosticsModel::TEST_FAIL_STOP);
  }

  void RecordFailure(int outcome_code, const std::string& additional_info) {
    RecordOutcome(
        outcome_code, additional_info, DiagnosticsModel::TEST_FAIL_CONTINUE);
  }

  void RecordSuccess(const std::string& additional_info) {
    RecordOutcome(0, additional_info, DiagnosticsModel::TEST_OK);
  }

  void RecordOutcome(int outcome_code,
                     const std::string& additional_info,
                     DiagnosticsModel::TestResult result);

  static base::FilePath GetUserDefaultProfileDir();

  // DiagnosticsModel::TestInfo overrides
  int GetId() const override;
  std::string GetName() const override;
  std::string GetTitle() const override;
  DiagnosticsModel::TestResult GetResult() const override;
  std::string GetAdditionalInfo() const override;
  int GetOutcomeCode() const override;
  base::Time GetStartTime() const override;
  base::Time GetEndTime() const override;

 protected:
  // Derived classes override this method do perform the actual test.
  virtual bool ExecuteImpl(DiagnosticsModel::Observer* observer) = 0;

  // Derived classes may override this method to perform a recovery, if recovery
  // makes sense for the diagnostics test.
  virtual bool RecoveryImpl(DiagnosticsModel::Observer* observer);

  const DiagnosticsTestId id_;
  std::string additional_info_;
  int outcome_code_;
  DiagnosticsModel::TestResult result_;
  base::Time start_time_;
  base::Time end_time_;
};

}  // namespace diagnostics
#endif  // CHROME_BROWSER_DIAGNOSTICS_DIAGNOSTICS_TEST_H_
