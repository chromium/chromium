// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_FILE_UPLOAD_JOB_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_FILE_UPLOAD_JOB_H_

#include <string>
#include <string_view>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/proto/synced/upload_tracker.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// Restartable chunky upload, to reliably deliver a file to an external location
// (defined by the `Delegate`). It is created when certain upload is seen for
// the first time; however, it is not always at the 0 offset - it may have been
// started and made some progress before the device has been restarted.
class FileUploadJob {
 public:
  class TestEnvironment;

  // Base class for actual upload activity.
  // Pure virtual methods need to be implemented or mocked by the derived class,
  // which would also populate and interpret `origin_path`, `upload_parameters`,
  // `session_token` and `access_parameters`.
  class Delegate {
   public:
    using SmartPtr = std::unique_ptr<Delegate, base::OnTaskRunnerDeleter>;

    virtual ~Delegate();

    // Asynchronously initializes upload.
    // Calls back with `total` and `session_token` are set, or Status in case
    // of error.
    virtual void DoInitiate(
        std::string_view origin_path,
        std::string_view upload_parameters,
        base::OnceCallback<
            void(StatusOr<std::pair<int64_t /*total*/,
                                    std::string /*session_token*/>>)> cb) = 0;

    // Asynchronously uploads the next chunk.
    // Uses `scoped_reservation` to manage memory usage by data buffer.
    // Calls back with new `uploaded` and `session_token` (could be the same),
    // or Status in case of an error.
    virtual void DoNextStep(
        int64_t total,
        int64_t uploaded,
        std::string_view session_token,
        ScopedReservation scoped_reservation,
        base::OnceCallback<
            void(StatusOr<std::pair<int64_t /*uploaded*/,
                                    std::string /*session_token*/>>)> cb) = 0;

    // Asynchronously finalizes upload (once `uploaded` reached `total`).
    // Calls back with `access_parameters`, or Status in case of error.
    virtual void DoFinalize(
        std::string_view session_token,
        base::OnceCallback<void(StatusOr<std::string /*access_parameters*/>)>
            cb) = 0;

    // Asynchronously deletes the original file (either upon success, or when
    // the failure happened when `retry_count` dropped to 0). Doesn't wait for
    // completion and doesn't report the outcome.
    virtual void DoDeleteFile(std::string_view origin_path) = 0;

    // Returns weak pointer.
    base::WeakPtr<Delegate> GetWeakPtr();

   protected:
    Delegate();

    base::WeakPtrFactory<Delegate> weak_ptr_factory_{this};
  };

  // Singleton manager class responsible for keeping track of incoming jobs:
  // when an event shows up for processing, it needs to create a FileUploadJob
  // but only if the same job is not already in progress because of the same
  // upload events showing up earlier.
  class Manager {
   public:
    // Job lifetime (extended on every update).
    static constexpr base::TimeDelta kLifeTime = base::Hours(1);

    // Access single instance of the manager.
    static Manager* GetInstance();

    ~Manager();

    // Registers new job to the map or finds it, if the matching one is already
    // there. Hands over to the callback (if it is indeed new, the first action
    // needs to be initiation, otherwise processing based on the current state).
    // The returned job is owned by the `Manager`.
    void Register(Priority priority,
                  Record record_copy,
                  ::ash::reporting::LogUploadEvent log_upload_event,
                  Delegate::SmartPtr delegate,
                  base::OnceCallback<void(StatusOr<FileUploadJob*>)> result_cb);

    // Accessor.
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner() const;

   private:
    friend class base::NoDestructor<Manager>;
    friend class TestEnvironment;

    // Private constructor, used only internally and in TestEnvironment.
    Manager();

    // Task runner is not declared `const` for testing: to be able to reset it.
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
    SEQUENCE_CHECKER(manager_sequence_checker_);

    // Access manager instance, used only internally and in TestEnvironment.
    static std::unique_ptr<FileUploadJob::Manager>& instance_ref();

    // Map of the jobs in progress. Indexed by serialized UploadSettings proto -
    // so any new upload job with e.g. different `retry_count` or different
    // `upload_parameters` is accepted. Job is removed from the map once
    // respective event is confirmed.
    base::flat_map<std::string, std::unique_ptr<FileUploadJob>>
        uploads_in_progress_ GUARDED_BY_CONTEXT(manager_sequence_checker_);
  };

  // Helper class associating the job to the event currently being processed.
  class EventHelper {
   public:
    EventHelper(base::WeakPtr<FileUploadJob> job,
                Priority priority,
                Record record_copy,
                ::ash::reporting::LogUploadEvent log_upload_event);
    EventHelper(const EventHelper& other) = delete;
    EventHelper& operator=(const EventHelper& other) = delete;
    ~EventHelper();

    // FileUploadJob progresses based on the last recorded state.
    // Called once the job is located or created.
    // Uses `scoped_reservation` to manage memory usage by data buffer.
    // `done_cb_` is going to post update as the next tracking event.
    void Run(const ScopedReservation& scoped_reservation,
             base::OnceCallback<void(Status)> done_cb);

   private:
    // Complete and call `done_cb_` (with OK, if the event is accepted for
    // upload, error status if not).
    void Complete(Status status = Status::StatusOK());

    // Repost new event if successful, then complete.
    void RepostAndComplete();

    // Compose and post a retry event (with decremented retry count and no
    // tracker - thus initiating a new FileUploadJob).
    void PostRetry() const;

    SEQUENCE_CHECKER(sequence_checker_);

    const base::WeakPtr<FileUploadJob> job_;
    Priority priority_;
    Record record_copy_;
    ::ash::reporting::LogUploadEvent log_upload_event_;
    base::OnceCallback<void(Status)> done_cb_;

    base::WeakPtrFactory<EventHelper> weak_ptr_factory_{this};
  };

  // Constructor populates both `settings` and `tracker`, based on `LOG_UPLOAD`
  // event. When upload is going to be started, `tracker` is empty yet.
  FileUploadJob(const UploadSettings& settings,
                const UploadTracker& tracker,
                Delegate::SmartPtr delegate);
  FileUploadJob(const FileUploadJob& other) = delete;
  FileUploadJob& operator=(const FileUploadJob& other) = delete;
  ~FileUploadJob();

  // The Job activity starts with a call to `Initiate`: before that time
  // `tracker_` is populated for the first time to enable continuous workflow,
  // including `session_token` that must be set and identifies the external
  // access on the next steps.
  // Then the Job proceeds with one or more calls to `NextStep`: after every
  // step `tracker_` is updated and `scoped_reservation` is used to manage
  // memory usage by data buffer. Note that `session_token` might change if it
  // is necessary to track the progress externally.
  // After the Job finished uploading, it calls `Finalize`, setting up
  // `access_parameters` or error status in `tracker_`.
  // The Job can be recreated and restarted from any step based on input
  // `tracker` parameter (for example, when a device is restarted, `tracker`
  // is loaded from LOG_UPLOAD event).
  // All 3 APIs below execute asynchronously; if is safe to call them repeatedly
  // (if the job is executing an API call, the new one will be a no-op. It is
  // possible (but not necessary) to provide `done_cb` callback to be called
  // once finished - this option is mostly used for testing.
  void Initiate(base::OnceClosure done_cb = base::DoNothing());
  void NextStep(const ScopedReservation& scoped_reservation,
                base::OnceClosure done_cb = base::DoNothing());
  void Finalize(base::OnceClosure done_cb = base::DoNothing());

  // Test-only explicit setter of the event helper.
  void SetEventHelperForTest(std::unique_ptr<EventHelper> event_helper);

  // Accessors.
  EventHelper* event_helper() const;
  const UploadSettings& settings() const;
  const UploadTracker& tracker() const;
  base::WeakPtr<FileUploadJob> GetWeakPtr();

 private:
  // The next three methods complement `Initiate`, `NextStep` and `Finalize` -
  // they are invoked after delegate calls are executed on a thread pool, and
  // resume execution on the Job's default task runner.
  void DoneInitiate(
      base::ScopedClosureRunner done,
      StatusOr<std::pair<int64_t /*total*/, std::string /*session_token*/>>
          result);
  void DoneNextStep(
      base::ScopedClosureRunner done,
      StatusOr<std::pair<int64_t /*uploaded*/, std::string /*session_token*/>>
          result);
  void DoneFinalize(base::ScopedClosureRunner done,
                    StatusOr<std::string /*access_parameters*/> result);

  // Creates scoped closure runner that augments `done_cb` with the ability to
  // asynchronously delete the original file upon success or the failure that
  // happened when `retry_count` dropped to 0.
  base::ScopedClosureRunner CompletionCb(base::OnceClosure done_cb);

  // Post event.
  static void AddRecordToStorage(Priority priority,
                                 Record record_copy,
                                 base::OnceCallback<void(Status)> done_cb);

  SEQUENCE_CHECKER(job_sequence_checker_);

  // Delegate that performs actual actions.
  const Delegate::SmartPtr delegate_;

  // Job parameters matching the event.
  const UploadSettings settings_;
  UploadTracker tracker_ GUARDED_BY_CONTEXT(job_sequence_checker_);

  // Event helper instance for event currently being processed by the job
  // (null when no event is processed).
  std::unique_ptr<EventHelper> event_helper_
      GUARDED_BY_CONTEXT(job_sequence_checker_);

  // Expiration timer of the job. Once the timer fires, the job is unregistered
  // and destructed. The timer is reset every time the job is accessed.
  base::RetainingOneShotTimer timer_ GUARDED_BY_CONTEXT(job_sequence_checker_);

  // Weak pointer factory to be used for returning from async calls to Delegate.
  // Must be the last member in the class.
  base::WeakPtrFactory<FileUploadJob> weak_ptr_factory_{this};
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_FILE_UPLOAD_JOB_H_
