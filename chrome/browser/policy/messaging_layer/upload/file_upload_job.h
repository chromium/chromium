// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_FILE_UPLOAD_JOB_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_FILE_UPLOAD_JOB_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/thread_annotations.h"
#include "components/reporting/proto/synced/upload_tracker.pb.h"
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
    virtual ~Delegate() = default;

    // Initializes upload.
    // Populates `total` and `session_token`, sets `uploaded` to 0.
    virtual Status DoInitiate(base::StringPiece origin_path,        // IN
                              base::StringPiece upload_parameters,  // IN
                              int64_t* total,                       // OUT
                              std::string* session_token            // OUT
                              ) = 0;

    // Performs upload of the next chunk.
    // Updates `uploaded` and optionally `session_token`.
    // Returns status in case of an error.
    virtual Status DoNextStep(int64_t total,              // IN
                              int64_t* uploaded,          // INOUT
                              std::string* session_token  // INOUT
                              ) = 0;

    // Finalizes upload (once uploaded reached total).
    // Populates `access_parameters`.
    // Returns status in case of an error.
    virtual Status DoFinalize(base::StringPiece session_token,  // IN
                              std::string* access_parameters    // OUT
                              ) = 0;

   protected:
    Delegate() = default;
  };

  // Constructor populates both `settings` and `tracker`, based on `LOG_UPLOAD`
  // event. When upload is going to be started, `tracker` is empty yet.
  FileUploadJob(const UploadSettings& settings,
                const UploadTracker& tracker,
                Delegate* delegate);  // not owned, must outlive the Job!
  FileUploadJob(const FileUploadJob& other) = delete;
  FileUploadJob& operator=(const FileUploadJob& other) = delete;
  ~FileUploadJob();

  // The Job activity starts with a call to `Initiate`: before that time
  // `tracker_` is populated for the first time to enable continuous workflow,
  // including `session_token` that must be set and identifies the external
  // access on the next steps.
  // Then the Job proceeds with one or more calls to `NextStep`: after every
  // step `tracker_` is updated. Note that `session_token` might change if it
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
  void NextStep(base::OnceClosure done_cb = base::DoNothing());
  void Finalize(base::OnceClosure done_cb = base::DoNothing());

  // Accessors.
  const UploadSettings& settings() const;
  const UploadTracker& tracker() const;
  base::WeakPtr<FileUploadJob> GetWeakPtr();

 private:
  // The next three methods complement `Initiate`, `NextStep` and `Finalize` -
  // they are called after delegate calls are executed on a thread pool, and
  // resume execution on the Job's default task runner.
  void DoneInitiate(base::ScopedClosureRunner done,
                    Status status,
                    int64_t total,
                    std::string session_token);
  void DoneNextStep(base::ScopedClosureRunner done,
                    Status status,
                    int64_t uploaded,
                    std::string session_token);
  void DoneFinalize(base::ScopedClosureRunner done,
                    Status status,
                    std::string access_parameters);

  // Unowned delegate that performs actual actions.
  // It must outlive the job (the same delegate may be used by multiple jobs).
  const base::raw_ptr<Delegate> delegate_;

  SEQUENCE_CHECKER(job_sequence_checker_);

  // Note: Cannot be const, since `retry_count` needs to be decremented.
  UploadSettings settings_ GUARDED_BY_CONTEXT(job_sequence_checker_);

  UploadTracker tracker_ GUARDED_BY_CONTEXT(job_sequence_checker_);

  // Flag indicating that the job is performing an action.
  // Any other action is rejected while the flag is set.
  bool in_action_ GUARDED_BY_CONTEXT(job_sequence_checker_) = false;

  // Weak pointer factory to be used for returning from async calls to Delegate.
  // Must be the last member in the class.
  base::WeakPtrFactory<FileUploadJob> weak_ptr_factory_{this};
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_FILE_UPLOAD_JOB_H_
