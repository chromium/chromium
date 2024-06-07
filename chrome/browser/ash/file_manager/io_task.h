// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_IO_TASK_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_IO_TASK_H_

#include <cstddef>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "storage/browser/file_system/file_system_url.h"

class Profile;

namespace file_manager::io_task {

enum class State {
  // Task has been queued, but not yet started.
  kQueued,

  // Task has started, but some initial scanning is performed.
  kScanning,

  // Task is currently running.
  kInProgress,

  // Task is currently paused.
  kPaused,

  // Task has been successfully completed.
  kSuccess,

  // Task has completed with errors.
  kError,

  // Task has failed to finish due to missing password.
  kNeedPassword,

  // Task has been canceled without finishing.
  kCancelled,
};

std::ostream& operator<<(std::ostream& out, State state);

enum class OperationType {
  kCopy,
  kDelete,
  kEmptyTrash,
  kExtract,
  kMove,

  // This restores to the location supplied in the .trashinfo folder, recreating
  // the parent hierarchy as required. As .Trash folders reside on the same
  // filesystem as trashed files, this implies an intra filesystem move.
  kRestore,

  // This restores to a supplied destination only extracting the file name to
  // properly name the destination file. The destination folder is expected to
  // exist and items can be restored cross filesystem.
  kRestoreToDestination,
  kTrash,
  kZip,
};

std::ostream& operator<<(std::ostream& out, OperationType op);

// The type of Data Protection policy error that occurred.
enum class PolicyErrorType {
  // Error caused by Data Leak Prevention policy.
  kDlp,

  // Error caused by Enterprise Connectors policy.
  kEnterpriseConnectors,

  // Error caused by Data Leak Prevention warning timing out.
  kDlpWarningTimeout,
};

// Holds information about data protection policy errors.
struct PolicyError {
  // Type of the error.
  PolicyErrorType type;
  // The number of files blocked by the policy.
  size_t blocked_files = 0;
  // The name of the first file among those under block restriction. Used for
  // notifications.
  std::string file_name;
  // Normally the review button is only shown when `blocked_files` is >1, this
  // option allows to force the display of the review button irrespective of
  // other conditions.
  bool always_show_review = false;

  bool operator==(const PolicyError& other) const;
  bool operator!=(const PolicyError& other) const;
};

// Unique identifier for any type of task.
using IOTaskId = uint64_t;

// I/O task state::PAUSED parameters when paused to resolve a file name
// conflict. Currently, only supported by CopyOrMoveIOTask and
// RestoreToDestinationIOTask.
struct ConflictPauseParams {
  // The conflict file name.
  std::string conflict_name;

  // True if the conflict file name is a directory.
  bool conflict_is_directory = false;

  // Set true if there are potentially multiple conflicted file names.
  bool conflict_multiple = false;

  // The conflict copy or move target URL.
  std::string conflict_target_url;

  bool operator==(const ConflictPauseParams& other) const;
};

// I/O task state::PAUSED parameters when paused to show a policy warning.
// Currently, only supported by CopyOrMovePolicyIOTask and
// RestoreToDestinationIOTask.
struct PolicyPauseParams {
  // One of kDlp, kEnterpriseConnectors.
  policy::Policy type;
  // The number of files under warning restriction. Needed to show correct
  // notifications.
  size_t warning_files_count = 0;
  // The name of the first file among those under warning restriction. Used for
  // notifications.
  std::string file_name;
  // Normally the review button is only shown when `warning_files_count` is >1,
  // this option allows to force the display of the review button irrespective
  // of other conditions.
  bool always_show_review = false;

  bool operator==(const PolicyPauseParams& other) const;
};

// I/O task state::PAUSED parameters. Only one of conflict or policy params
// should be set.
struct PauseParams {
  PauseParams();

  PauseParams(const PauseParams& other);
  PauseParams& operator=(const PauseParams& other);

  PauseParams(PauseParams&& other);
  PauseParams& operator=(PauseParams&& other);

  bool operator==(const PauseParams& other) const;

  ~PauseParams();

  // Set iff pausing due to name conflict.
  std::optional<ConflictPauseParams> conflict_params;
  // Set iff pausing due to a policy warning.
  std::optional<PolicyPauseParams> policy_params;
};

// Resume I/O task parameters when paused because of a name conflict.
struct ConflictResumeParams {
  // How to resolve a CopyOrMoveIOTask file name conflict: either 'keepboth'
  // or 'replace'.
  std::string conflict_resolve;

  // True if |conflict_resolve| should apply to future file name conflicts.
  bool conflict_apply_to_all = false;
};

// Resume I/O task parameters when paused because of a policy.
struct PolicyResumeParams {
  // One of kDlp, kEnterpriseConnectors.
  policy::Policy type;
};

// Resume I/O task parameters.
struct ResumeParams {
  ResumeParams();

  ResumeParams(const ResumeParams& other);
  ResumeParams& operator=(const ResumeParams& other);

  ResumeParams(ResumeParams&& other);
  ResumeParams& operator=(ResumeParams&& other);

  ~ResumeParams();

  // Set iff paused due to name conflict.
  std::optional<ConflictResumeParams> conflict_params;
  // Set iff paused due to a policy warning.
  std::optional<PolicyResumeParams> policy_params;
};

// Represents the status of a particular entry in an I/O task.
struct EntryStatus {
  EntryStatus(storage::FileSystemURL file_url,
              std::optional<base::File::Error> file_error,
              std::optional<storage::FileSystemURL> source_url = std::nullopt);
  ~EntryStatus();

  EntryStatus(EntryStatus&& other);
  EntryStatus& operator=(EntryStatus&& other);

  // The entry FileSystemURL.
  storage::FileSystemURL url;

  // May be empty if the entry has not been fully processed yet.
  std::optional<base::File::Error> error;

  // The source from which the entry identified by `url` is generated. May be
  // empty if not relevant.
  std::optional<storage::FileSystemURL> source_url;

  // True if entry is a directory when its metadata is processed.
  bool is_directory = false;
};

// Represents the current progress of an I/O task.
class ProgressStatus {
 public:
  // Out-of-line constructors to appease the style linter.
  ProgressStatus();
  ProgressStatus(const ProgressStatus& other) = delete;
  ProgressStatus& operator=(const ProgressStatus& other) = delete;
  ~ProgressStatus();

  // Allow ProgressStatus to be moved.
  ProgressStatus(ProgressStatus&& other);
  ProgressStatus& operator=(ProgressStatus&& other);

  // True if the task is in kPaused state.
  bool IsPaused() const;

  // True if the task is in a terminal state and won't receive further updates.
  bool IsCompleted() const;

  // True if the task is paused due to a data protection policy warning.
  bool HasWarning() const;

  // True if the task completed with security errors due to Data Leak Prevention
  // or Enterprise Connectors policies.
  bool HasPolicyError() const;

  // True if the task is in scanning state.
  bool IsScanning() const;

  // Returns a default method for obtaining the source name.
  std::string GetSourceName(Profile* profile) const;

  // Setter for the destination folder and the destination volume id.
  void SetDestinationFolder(storage::FileSystemURL folder,
                            Profile* profile = nullptr);
  const storage::FileSystemURL& GetDestinationFolder() const {
    return destination_folder_;
  }
  const std::string& GetDestinationVolumeId() const {
    return destination_volume_id_;
  }

  // Task state.
  State state;

  // Information about policy errors that occurred, if any. Empty otherwise.
  // Can be set only if Data Leak Prevention or Enterprise Connectors policies
  // apply.
  std::optional<PolicyError> policy_error;

  // I/O Operation type (e.g. copy, move).
  OperationType type;

  // Files the operation processes.
  std::vector<EntryStatus> sources;

  // The file name to use when reporting progress.
  std::string source_name;

  // Entries created by the I/O task. These files aren't necessarily related to
  // |sources|.
  std::vector<EntryStatus> outputs;

  // I/O task state::PAUSED parameters.
  PauseParams pause_params;

  // ProgressStatus over all |sources|.
  int64_t bytes_transferred = 0;

  // Total size of all |sources|.
  int64_t total_bytes = 0;

  // The task id for this progress status.
  IOTaskId task_id = 0;

  // The estimate time to finish the operation.
  double remaining_seconds = 0;

  // Number of `sources` scanned - must be <= `sources.size()`. Only used when
  // in kScanning `state`. When scanning files, the progress is roughly the
  // percentage of the number of scanned items out of the total items. This
  // isn't always accurate, e.g. when uploading entire folders or because some
  // items are not scanned at all. The goal is to show the user that some
  // progress is happening.
  size_t sources_scanned = 0;

  // Whether notifications should be shown on progress status.
  bool show_notification = true;

  // List of files skipped during the operation because we couldn't decrypt
  // them.
  std::vector<storage::FileSystemURL> skipped_encrypted_files;

 private:
  // Optional destination folder for operations that transfer files to a
  // directory (e.g. copy or move).
  storage::FileSystemURL destination_folder_;

  // Volume id of the destination directory for operations that transfer files
  // to a directory (e.g. copy or move).
  std::string destination_volume_id_;
};

// An IOTask represents an I/O operation over multiple files, and is responsible
// for executing the operation and providing progress/completion reports.
class IOTask {
 public:
  IOTask() = delete;
  IOTask(const IOTask& other) = delete;
  IOTask& operator=(const IOTask& other) = delete;
  virtual ~IOTask() = default;

  using ProgressCallback = base::RepeatingCallback<void(const ProgressStatus&)>;
  using CompleteCallback = base::OnceCallback<void(ProgressStatus)>;

  // Executes the task. |progress_callback| should be called every so often to
  // give updates, and |complete_callback| should be only called once at the end
  // to signify completion with a |kSuccess|, |kError| or |kCancelled| state.
  // |progress_callback| should be called on the same sequence Execute() was.
  virtual void Execute(ProgressCallback progress_callback,
                       CompleteCallback complete_callback) = 0;

  // Pauses a task.
  virtual void Pause(PauseParams params);

  // Resumes a task.
  virtual void Resume(ResumeParams params);

  // Cancels the task. This should set the progress state to be |kCancelled|,
  // but not call any of Execute()'s callbacks. The task will be deleted
  // synchronously after this call returns.
  virtual void Cancel() = 0;

  // Aborts the task because of policy error. This should set the progress state
  // to be |kError| with `policy_error` but not call any of Execute()'s
  // callbacks. The task will be deleted synchronously after this call returns.
  virtual void CompleteWithError(PolicyError policy_error);

  // Gets the current progress status of the task.
  const ProgressStatus& progress() { return progress_; }

  // Sets the task id.
  void SetTaskID(IOTaskId task_id) { progress_.task_id = task_id; }

 protected:
  explicit IOTask(bool show_notification) {
    progress_.show_notification = show_notification;
  }

  ProgressStatus progress_;
};

// No-op IO Task for testing.
// TODO(austinct): Move into io_task_controller_unittest file when the other
// IOTasks have been implemented.
class DummyIOTask : public IOTask {
 public:
  DummyIOTask(std::vector<storage::FileSystemURL> source_urls,
              storage::FileSystemURL destination_folder,
              OperationType type,
              bool show_notification = true,
              bool progress_succeeds = true);
  ~DummyIOTask() override;

  // IOTask overrides:
  void Execute(ProgressCallback progress_callback,
               CompleteCallback complete_callback) override;
  void Pause(PauseParams pause_params) override;
  void Resume(ResumeParams resume_params) override;
  void Cancel() override;
  void CompleteWithError(PolicyError policy_error) override;

 private:
  void DoProgress();
  void DoComplete();

  ProgressCallback progress_callback_;
  CompleteCallback complete_callback_;

  // Whether progressing the task should automatically complete it with
  // kSuccess.
  bool progress_succeeds_;

  base::WeakPtrFactory<DummyIOTask> weak_ptr_factory_{this};
};

}  // namespace file_manager::io_task

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_IO_TASK_H_
