// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/cros_action_history/cros_action_recorder.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/ash/app_list/search/cros_action_history/cros_action.pb.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/metrics/structured/structured_events.h"

namespace app_list {
namespace {

constexpr int kSecondsPerDay = 86400;
// If |CrOSActionRecorder::actions_| gets longer than this, force a Flush to
// disk.
constexpr int kActionLimitInMemory = 3600;
// If current file already contains more record than this, skip the rest for
// that day.
constexpr int kActionLimitPerFile = 100000;

// Prefixes for CrOSActionRecorder::RecordAction.
constexpr char kSettingChangedPrefix[] = "SettingsChanged-";
constexpr char kSearchResultLaunchedPrefix[] = "SearchResultLaunched-";
constexpr char kFileOpenedPrefix[] = "FileOpened-";
constexpr char kTabNavigatedPrefix[] = "TabNavigated-";
constexpr char kTabReactivatedPrefix[] = "TabReactivated-";
constexpr char kTabOpenedPrefix[] = "TabOpened-";

// Enables Hashed Logging for CrOSAction.
BASE_FEATURE(kCrOSActionStructuredMetrics,
             "CrOSActionStructuredMetrics",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Represents the events of the CrOSActionRecorder.
// This enum is used for a histogram and should not be renumbered and the old
// values should not be reused.
enum CrOSActionRecorderEvent {
  kDisabled = 0,
  kRecordAction = 1,
  kFlushToDisk = 2,
  kReadFromFileFail = 3,
  kParseFromStringFail = 4,
  kCreateDirectoryFail = 5,
  kWriteFileAtomicallyFail = 6,
  kStructuredMetricsLogged = 7,
  kMaxValue = kStructuredMetricsLogged,
};

// Records CrOSActionRecorder event.
void RecordCrOSActionEvent(const CrOSActionRecorderEvent val) {
  UMA_HISTOGRAM_ENUMERATION("Cros.CrOSActionRecorderEvent", val,
                            CrOSActionRecorderEvent::kMaxValue);
}

// Append the |actions| to the |action_filepath|.
void SaveToDiskOnWorkerThread(const CrOSActionHistoryProto actions,
                              const base::FilePath action_filepath) {
  // Loads proto string from local disk.
  std::string proto_str;
  if (!base::ReadFileToString(action_filepath, &proto_str)) {
    proto_str.clear();
    RecordCrOSActionEvent(CrOSActionRecorderEvent::kReadFromFileFail);
  }

  CrOSActionHistoryProto actions_to_write;
  if (!actions_to_write.ParseFromString(proto_str)) {
    actions_to_write.Clear();
    RecordCrOSActionEvent(CrOSActionRecorderEvent::kParseFromStringFail);
  }

  if (actions_to_write.actions_size() > kActionLimitPerFile)
    return;

  actions_to_write.MergeFrom(actions);
  const std::string proto_str_to_write = actions_to_write.SerializeAsString();

  // Create directory if it's not there yet.
  const bool create_directory_success =
      base::CreateDirectory(action_filepath.DirName());
  if (!create_directory_success) {
    RecordCrOSActionEvent(CrOSActionRecorderEvent::kCreateDirectoryFail);
    DCHECK(create_directory_success)
        << "Error create directory for " << action_filepath;
  }

  const bool write_success = base::ImportantFileWriter::WriteFileAtomically(
      action_filepath, proto_str_to_write, "CrOSActionHistory");
  if (!write_success) {
    RecordCrOSActionEvent(CrOSActionRecorderEvent::kWriteFileAtomicallyFail);
    DCHECK(write_success) << "Error writing action_file " << action_filepath;
  }
}

void DeleteExistingLog(const base::FilePath model_dir) {
  // We don't want anyone accidentally deletes everything in the home directory.
  if (model_dir.BaseName().MaybeAsASCII() !=
      CrOSActionRecorder::kActionHistoryDir)
    return;

  base::DeletePathRecursively(model_dir);
}

void CopyToDownloadDir(const base::FilePath model_dir,
                       const base::FilePath proto_file_in_download_dir) {
  // If |model_dir| doesn't exist, no action should be taken.
  if (!base::DirectoryExists(model_dir))
    return;

  // Get all filenames in sorted order.
  std::vector<base::FilePath> filenames;
  base::FileEnumerator model_files(model_dir, false,
                                   base::FileEnumerator::FILES);
  for (base::FilePath filename = model_files.Next(); !filename.empty();
       filename = model_files.Next()) {
    filenames.push_back(filename);
  }

  // The basename of the file is the day (in integer form) of the time the file
  // is written. We sort the basenames by length and then dictionary order which
  // is equivalent to integer order.
  auto sort_by_day = [](const base::FilePath& a, const base::FilePath& b) {
    const std::string basename_a = a.BaseName().MaybeAsASCII();
    const std::string basename_b = b.BaseName().MaybeAsASCII();
    return basename_a.size() < basename_b.size() ||
           (basename_a.size() == basename_b.size() && basename_a < basename_b);
  };

  std::sort(filenames.begin(), filenames.end(), sort_by_day);

  // Read all files and merge them into one proto.
  CrOSActionHistoryProto merged_proto;
  for (const auto& filename : filenames) {
    std::string proto_str;
    if (!base::ReadFileToString(filename, &proto_str))
      continue;

    CrOSActionHistoryProto proto;
    if (!proto.ParseFromString(proto_str))
      continue;

    merged_proto.MergeFrom(proto);
  }

  // Write to download directory.
  const bool write_success = base::ImportantFileWriter::WriteFileAtomically(
      proto_file_in_download_dir, merged_proto.SerializeAsString(),
      "CrOSActionHistory");
  if (!write_success) {
    RecordCrOSActionEvent(CrOSActionRecorderEvent::kWriteFileAtomicallyFail);
    DCHECK(write_success) << "Copying data to download directory failed.";
  }
}

// Finds condition value with given key; return -1 if not existed.
int FindWithDefault(const std::map<std::string, int>& conditions,
                    const std::string& key) {
  const auto find_it = conditions.find(key);
  if (find_it != conditions.end()) {
    return find_it->second;
  } else {
    return -1;
  }
}

// Returns false if |input| doesn't start with prefix.
// Returns true and delete prefix from |input| otherwise.
bool ConsumePrefix(std::string* input, const std::string& prefix) {
  if (input->find(prefix) != 0) {
    return false;
  }
  *input = input->substr(prefix.size());
  return true;
}

}  // namespace

constexpr base::TimeDelta CrOSActionRecorder::kSaveInternal;
constexpr char CrOSActionRecorder::kActionHistoryDir[];
constexpr char CrOSActionRecorder::kActionHistoryBasename[];

CrOSActionRecorder::CrOSActionRecorder()
    : CrOSActionRecorder(ProfileManager::GetActiveUserProfile()) {}

CrOSActionRecorder::~CrOSActionRecorder() = default;

CrOSActionRecorder* CrOSActionRecorder::GetCrosActionRecorder() {
  static base::NoDestructor<CrOSActionRecorder> recorder;
  return recorder.get();
}

void CrOSActionRecorder::RecordAction(
    const CrOSAction& action,
    const std::map<std::string, int>& conditions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (structured_metrics_enabled_) {
    LogCrOSActionAsStructuredMetrics(action, conditions);
  }

  // Skip if the type is not kLogWithHash or kLogWithoutHash.
  if (type_ != CrOSActionRecorderType::kLogWithHash &&
      type_ != CrOSActionRecorderType::kLogWithoutHash)
    return;

  RecordCrOSActionEvent(CrOSActionRecorderEvent::kRecordAction);
  CrOSActionProto& cros_action_proto = *actions_.add_actions();

  const bool should_hash = type_ == CrOSActionRecorderType::kLogWithHash;

  // Record action.
  cros_action_proto.set_action_name(
      MaybeHashed(std::get<0>(action), should_hash));
  cros_action_proto.set_secs_since_epoch(base::Time::Now().ToDoubleT());

  // Record conditions.
  for (const auto& pair : conditions) {
    auto& condition = *cros_action_proto.add_conditions();
    condition.set_name(MaybeHashed(pair.first, should_hash));
    condition.set_value(pair.second);
  }

  // May flush to disk.
  MaybeFlushToDisk();
}

CrOSActionRecorder::CrOSActionRecorder(Profile* profile) {
  Init(profile);
}

void CrOSActionRecorder::Init(Profile* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  last_save_timestamp_ = base::Time::Now();
  last_action_time_ = base::Time::Now();
  sequence_id_ = 0;

  SetCrOSActionRecorderType();

  // Enable structured metrics only if:
  // (1) profile is available.
  // (2) enabled from finch.
  // (3) not disabled by the user manually.
  structured_metrics_enabled_ =
      profile && base::FeatureList::IsEnabled(kCrOSActionStructuredMetrics) &&
      type_ != CrOSActionRecorderType::kStructuredMetricsDisabled;

  // Do not record if the profile is empty.
  if (!profile) {
    type_ = CrOSActionRecorderType::kDefault;
  }
  // Skip if if the feature is not enabled.
  if (type_ == CrOSActionRecorderType::kDefault)
    return;

  model_dir_ =
      profile->GetPath().AppendASCII(CrOSActionRecorder::kActionHistoryDir);

  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  // Delete all cros action log if it's disabled.
  if (type_ == CrOSActionRecorderType::kLogDisabled) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&DeleteExistingLog, model_dir_));
    return;
  }

  // Copy to download directory if required.
  if (type_ == CrOSActionRecorderType::kCopyToDownloadDir) {
    const base::FilePath filename_in_download_dir =
        DownloadPrefs(profile)
            .GetDefaultDownloadDirectoryForProfile()
            .AppendASCII(CrOSActionRecorder::kActionHistoryBasename);

    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&CopyToDownloadDir, model_dir_,
                                          filename_in_download_dir));
  }
}

void CrOSActionRecorder::MaybeFlushToDisk() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (actions_.actions().empty())
    return;

  const base::Time now = base::Time::Now();
  if (now - last_save_timestamp_ >= kSaveInternal ||
      actions_.actions_size() > kActionLimitInMemory) {
    RecordCrOSActionEvent(CrOSActionRecorderEvent::kFlushToDisk);

    last_save_timestamp_ = now;
    // Writes the predictor proto to disk asynchronously.
    const std::string day = base::NumberToString(
        static_cast<int>(now.ToDoubleT() / kSecondsPerDay));
    const base::FilePath action_filepath = model_dir_.AppendASCII(day);

    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SaveToDiskOnWorkerThread,
                                  std::move(actions_), action_filepath));
    actions_.Clear();
  }
}

void CrOSActionRecorder::SetCrOSActionRecorderType() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(ash::switches::kEnableCrOSActionRecorder)) {
    const std::string& cros_action_flag = command_line->GetSwitchValueASCII(
        ash::switches::kEnableCrOSActionRecorder);

    if (cros_action_flag == ash::switches::kCrOSActionRecorderWithHash) {
      type_ = CrOSActionRecorderType::kLogWithHash;
    } else if (cros_action_flag ==
               ash::switches::kCrOSActionRecorderWithoutHash) {
      type_ = CrOSActionRecorderType::kLogWithoutHash;
    } else if (cros_action_flag ==
               ash::switches::kCrOSActionRecorderCopyToDownloadDir) {
      type_ = CrOSActionRecorderType::kCopyToDownloadDir;
    } else if (cros_action_flag == ash::switches::kCrOSActionRecorderDisabled) {
      type_ = CrOSActionRecorderType::kLogDisabled;
    } else if (cros_action_flag ==
               ash::switches::kCrOSActionRecorderStructuredDisabled) {
      type_ = CrOSActionRecorderType::kStructuredMetricsDisabled;
    }
  }
}

void CrOSActionRecorder::LogCrOSActionAsStructuredMetrics(
    const CrOSAction& action,
    const std::map<std::string, int>& conditions) {
  RecordCrOSActionEvent(CrOSActionRecorderEvent::kStructuredMetricsLogged);
  // Updates sequence_id and last_action_time.
  ++sequence_id_;
  const int64_t time_since_last_action =
      (base::Time::Now() - last_action_time_).InMilliseconds();
  last_action_time_ = base::Time::Now();

  std::string action_name = std::get<0>(action);

  if (ConsumePrefix(&action_name, kSearchResultLaunchedPrefix)) {
    // SearchReultLaunched.
    metrics::structured::events::v2::hindsight::
        CrOSActionEvent_SearchResultLaunched()
            .SetQuery(
                base::NumberToString(FindWithDefault(conditions, "Query")))
            .SetResultType(FindWithDefault(conditions, "ResultType"))
            .SetSearchResultId(action_name)
            .SetSequenceId(sequence_id_)
            .SetTimeSinceLastAction(time_since_last_action)
            .Record();
  } else if (ConsumePrefix(&action_name, kFileOpenedPrefix)) {
    // FileOpened.
    metrics::structured::events::v2::hindsight::CrOSActionEvent_FileOpened()
        .SetFilename(action_name)
        .SetOpenType(FindWithDefault(conditions, "open_type"))
        .SetSequenceId(sequence_id_)
        .SetTimeSinceLastAction(time_since_last_action)
        .Record();
  } else if (ConsumePrefix(&action_name, kSettingChangedPrefix)) {
    // SettingChanged.
    metrics::structured::events::v2::hindsight::CrOSActionEvent_SettingChanged()
        .SetSettingId(FindWithDefault(conditions, "SettingId"))
        .SetSettingType(FindWithDefault(conditions, "SettingType"))
        .SetPreviousValue(FindWithDefault(conditions, "PreviousValue"))
        .SetCurrentValue(FindWithDefault(conditions, "CurrentValue"))
        .SetSequenceId(sequence_id_)
        .SetTimeSinceLastAction(time_since_last_action)
        .Record();
  } else if (ConsumePrefix(&action_name, kTabNavigatedPrefix)) {
    // Navigate to a new tab.
    metrics::structured::events::v2::hindsight::
        CrOSActionEvent_TabEvent_TabNavigated()
            .SetURL(action_name)
            .SetVisibility(FindWithDefault(conditions, "Visibility"))
            .SetPageTransition(FindWithDefault(conditions, "PageTransition"))
            .SetSequenceId(sequence_id_)
            .SetTimeSinceLastAction(time_since_last_action)
            .Record();
  } else if (ConsumePrefix(&action_name, kTabReactivatedPrefix)) {
    // Reactivate an old tab.
    metrics::structured::events::v2::hindsight::
        CrOSActionEvent_TabEvent_TabReactivated()
            .SetURL(action_name)
            .SetSequenceId(sequence_id_)
            .SetTimeSinceLastAction(time_since_last_action)
            .Record();
  } else if (ConsumePrefix(&action_name, kTabOpenedPrefix)) {
    // Open a tab from current tab.
    // current tab is stored as a condition with value -1.
    std::string current_url;
    for (const auto& condition : conditions) {
      if (std::get<1>(condition) == -1) {
        current_url = std::get<0>(condition);
      }
    }

    metrics::structured::events::v2::hindsight::
        CrOSActionEvent_TabEvent_TabOpened()
            .SetURL(current_url)
            .SetURLOpened(action_name)
            .SetWindowOpenDisposition(
                FindWithDefault(conditions, "WindowOpenDisposition"))
            .SetSequenceId(sequence_id_)
            .SetTimeSinceLastAction(time_since_last_action)
            .Record();
  }
}

std::string CrOSActionRecorder::MaybeHashed(const std::string& input,
                                            const bool should_hash) {
  return should_hash ? base::NumberToString(base::HashMetricName(input))
                     : input;
}

}  // namespace app_list
