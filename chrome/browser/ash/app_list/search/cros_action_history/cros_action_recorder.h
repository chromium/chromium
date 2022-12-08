// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_CROS_ACTION_HISTORY_CROS_ACTION_RECORDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_CROS_ACTION_HISTORY_CROS_ACTION_RECORDER_H_

#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/cros_action_history/cros_action.pb.h"

class Profile;

namespace app_list {

namespace test {
class CrOSActionRecorderTest;
class CrOSActionRecorderTabTrackerTest;
}  // namespace test

class CrOSActionHistoryProto;

// CrOSActionRecorder is a singleton that used to record any CrOSAction.
// CrOSAction may contains:
//   (1) App-launchings.
//   (2) File-openings.
//   (3) Settings.
//   (4) Tab-navigations.
// and so on.
class CrOSActionRecorder {
 public:
  using CrOSActionName = std::string;
  using CrOSAction = std::tuple<CrOSActionName>;

  CrOSActionRecorder();

  CrOSActionRecorder(const CrOSActionRecorder&) = delete;
  CrOSActionRecorder& operator=(const CrOSActionRecorder&) = delete;

  ~CrOSActionRecorder();
  // Get the pointer of the singleton.
  static CrOSActionRecorder* GetCrosActionRecorder();

  // Record a user |action| with |conditions|.
  void RecordAction(const CrOSAction& action,
                    const std::map<std::string, int>& conditions = {});

  // The sub-directory in profile path where the action history is stored.
  static constexpr char kActionHistoryDir[] = "cros_action_history";

  // The basename of the file for the copied action history.
  static constexpr char kActionHistoryBasename[] = "cros_action_history.pb";

 private:
  // Enum for recorder settings from flags.
  enum CrOSActionRecorderType {
    kDefault = 0,
    kLogWithHash = 1,
    kLogWithoutHash = 2,
    kCopyToDownloadDir = 3,
    kLogDisabled = 4,
    kStructuredMetricsDisabled = 5,
  };

  friend class test::CrOSActionRecorderTest;
  friend class test::CrOSActionRecorderTabTrackerTest;

  // kSaveInternal controls how often we save the action history to disk.
  static constexpr base::TimeDelta kSaveInternal = base::Hours(1);

  // Private constructor used for testing purpose. Which basically calls the
  // Init function.
  explicit CrOSActionRecorder(Profile* profile);

  // Does the actual initialization of CrOSActionRecorder.
  void Init(Profile* profile);

  // Saves the current |actions_| to disk and clear it when certain
  // criteria is met.
  void MaybeFlushToDisk();

  // Get CrOSActionRecorderType from
  // app_list_features::kEnableCrOSActionRecorder and set |should_log_|,
  // |should_hash_| accordingly.
  void SetCrOSActionRecorderType();

  // Re-maps action name and conditions into structured metrics.
  void LogCrOSActionAsStructuredMetrics(
      const CrOSAction& action,
      const std::map<std::string, int>& conditions);

  // Hashes the |input| if |should_hash| is true; otherwise return |input|.
  static std::string MaybeHashed(const std::string& input, bool should_hash);

  // Recorder type set from the flag.
  CrOSActionRecorderType type_ = CrOSActionRecorderType::kDefault;
  // Sequence of the cros action.
  int64_t sequence_id_ = 0;
  // Time of the last cros action.
  base::Time last_action_time_;
  // Only log structured metrics when enabled.
  bool structured_metrics_enabled_ = false;
  // The timestamp of last save to disk.
  base::Time last_save_timestamp_;
  // A list of actions since last save.
  CrOSActionHistoryProto actions_;
  // Path to save the actions history.
  base::FilePath model_dir_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_CROS_ACTION_HISTORY_CROS_ACTION_RECORDER_H_
