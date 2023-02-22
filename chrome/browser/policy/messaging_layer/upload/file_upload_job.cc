// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/file_upload_job.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/reporting/proto/synced/upload_tracker.pb.h"
#include "components/reporting/util/status.h"

namespace reporting {

// Manager implementation.

// static
FileUploadJob::Manager* FileUploadJob::Manager::GetInstance() {
  return instance_ref().get();
}

// static
std::unique_ptr<FileUploadJob::Manager>&
FileUploadJob::Manager::instance_ref() {
  static base::NoDestructor<std::unique_ptr<FileUploadJob::Manager>> instance(
      new FileUploadJob::Manager());
  return *instance;
}

FileUploadJob::Manager::Manager()
    : sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})) {
  DETACH_FROM_SEQUENCE(manager_sequence_checker_);
}

FileUploadJob::Manager::~Manager() = default;

void FileUploadJob::Manager::Register(
    const UploadSettings& settings,
    const UploadTracker& tracker,
    Delegate* delegate,
    base::OnceCallback<void(StatusOr<FileUploadJob*>)> result_cb) {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](Manager* self, const UploadSettings settings,
             const UploadTracker tracker, Delegate* delegate,
             base::OnceCallback<void(StatusOr<FileUploadJob*>)> result_cb) {
            // Serialize settings to get the map key.
            std::string serialized_settings;
            if (!settings.SerializeToString(&serialized_settings)) {
              std::move(result_cb).Run(Status(
                  error::INVALID_ARGUMENT, "Job settings failed to serialize"));
              return;
            }
            // Now add the job to the map.
            // Existing job is returned, new job is recorded and
            // returned.
            DCHECK_CALLED_ON_VALID_SEQUENCE(self->manager_sequence_checker_);
            auto it = self->uploads_in_progress_.find(serialized_settings);
            if (it == self->uploads_in_progress_.end()) {
              auto res = self->uploads_in_progress_.emplace(
                  serialized_settings,
                  std::make_unique<FileUploadJob>(settings, tracker, delegate));
              DCHECK(res.second);
              it = res.first;
              DCHECK_CALLED_ON_VALID_SEQUENCE(
                  it->second->job_sequence_checker_);
              it->second->timer_.Start(
                  FROM_HERE, kLifeTime,
                  base::BindRepeating(
                      [](Manager* self, std::string serialized_settings) {
                        // Locate the job in the map.
                        DCHECK_CALLED_ON_VALID_SEQUENCE(
                            self->manager_sequence_checker_);
                        auto it = self->uploads_in_progress_.find(
                            serialized_settings);
                        if (it == self->uploads_in_progress_.end()) {
                          // Not found.
                          return;
                        }
                        // Stop timer and remove the job from map (thus deleting
                        // it).
                        DCHECK_CALLED_ON_VALID_SEQUENCE(
                            it->second->job_sequence_checker_);
                        it->second->timer_.Stop();
                        self->uploads_in_progress_.erase(it);
                      },
                      base::Unretained(self), std::move(serialized_settings)));
            }
            std::move(result_cb).Run(it->second.get());
          },
          base::Unretained(this), settings, tracker, base::Unretained(delegate),
          std::move(result_cb)));
}

scoped_refptr<base::SequencedTaskRunner>
FileUploadJob::Manager::sequenced_task_runner() const {
  return sequenced_task_runner_;
}

// FileUploadJob implementation.

FileUploadJob::FileUploadJob(const UploadSettings& settings,
                             const UploadTracker& tracker,
                             Delegate* delegate)  // not owned!
    : delegate_(delegate), settings_(settings), tracker_(tracker) {
  DCHECK(delegate_);
}

FileUploadJob::~FileUploadJob() = default;

void FileUploadJob::Initiate(base::OnceClosure done_cb) {
  base::ScopedClosureRunner done(std::move(done_cb));
  DCHECK_CALLED_ON_VALID_SEQUENCE(job_sequence_checker_);
  if (tracker_.has_status()) {
    // Error detected earlier.
    return;
  }
  if (in_action_) {
    // The job is already executing some action, do nothing.
    return;
  }
  if (!tracker_.session_token().empty()) {
    Status{error::FAILED_PRECONDITION, "Job has already been initiated"}.SaveTo(
        tracker_.mutable_status());
    return;
  }
  if (settings_.retry_count() <= 0) {
    Status{error::OUT_OF_RANGE, "Too many upload attempts"}.SaveTo(
        tracker_.mutable_status());
    return;
  }
  settings_.set_retry_count(settings_.retry_count() - 1);
  in_action_ = true;
  if (timer_.IsRunning()) {
    timer_.Reset();
  }
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&Delegate::DoInitiate, base::Unretained(delegate_),
                     settings_.origin_path(), settings_.upload_parameters(),
                     base::BindPostTaskToCurrentDefault(base::BindOnce(
                         &FileUploadJob::DoneInitiate,
                         weak_ptr_factory_.GetWeakPtr(), std::move(done)))));
}

void FileUploadJob::DoneInitiate(
    base::ScopedClosureRunner done,
    StatusOr<std::pair<int64_t /*total*/, std::string /*session_token*/>>
        result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(job_sequence_checker_);
  in_action_ = false;
  if (!result.ok()) {
    result.status().SaveTo(tracker_.mutable_status());
    return;
  }
  int64_t total = 0L;
  base::StringPiece session_token;
  std::tie(total, session_token) = result.ValueOrDie();
  if (total <= 0L) {
    Status{error::FAILED_PRECONDITION, "Empty upload"}.SaveTo(
        tracker_.mutable_status());
    return;
  }
  if (session_token.empty()) {
    Status{error::FAILED_PRECONDITION, "Session token not created"}.SaveTo(
        tracker_.mutable_status());
    return;
  }
  tracker_.set_total(total);
  tracker_.set_uploaded(0L);
  tracker_.set_session_token(session_token.data(), session_token.size());
}

void FileUploadJob::NextStep(base::OnceClosure done_cb) {
  base::ScopedClosureRunner done(std::move(done_cb));
  DCHECK_CALLED_ON_VALID_SEQUENCE(job_sequence_checker_);
  if (tracker_.has_status()) {
    // Error detected earlier.
    return;
  }
  if (in_action_) {
    // The job is already executing some action, do nothing.
    return;
  }
  if (tracker_.session_token().empty()) {
    Status{error::FAILED_PRECONDITION, "Job has not been initiated yet"}.SaveTo(
        tracker_.mutable_status());
    return;
  }
  if (tracker_.uploaded() < 0L || tracker_.uploaded() > tracker_.total()) {
    Status{error::OUT_OF_RANGE,
           base::StrCat({"Uploaded ", base::NumberToString(tracker_.uploaded()),
                         " out of range"})}
        .SaveTo(tracker_.mutable_status());
    return;
  }
  if (tracker_.uploaded() == tracker_.total()) {
    // All done, Status is OK.
    return;
  }
  in_action_ = true;
  if (timer_.IsRunning()) {
    timer_.Reset();
  }
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&Delegate::DoNextStep, base::Unretained(delegate_),
                     tracker_.total(), tracker_.uploaded(),
                     tracker_.session_token(),
                     base::BindPostTaskToCurrentDefault(base::BindOnce(
                         &FileUploadJob::DoneNextStep,
                         weak_ptr_factory_.GetWeakPtr(), std::move(done)))));
}

void FileUploadJob::DoneNextStep(
    base::ScopedClosureRunner done,
    StatusOr<std::pair<int64_t /*uploaded*/, std::string /*session_token*/>>
        result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(job_sequence_checker_);
  in_action_ = false;
  if (!result.ok()) {
    result.status().SaveTo(tracker_.mutable_status());
    return;
  }
  int64_t uploaded = 0L;
  base::StringPiece session_token;
  std::tie(uploaded, session_token) = result.ValueOrDie();
  if (session_token.empty()) {
    Status{error::DATA_LOSS, "Job has lost session_token"}.SaveTo(
        tracker_.mutable_status());
    return;
  }
  if (uploaded < tracker_.uploaded()) {
    Status{error::DATA_LOSS,
           base::StrCat({"Job has backtracked from ",
                         base::NumberToString(tracker_.uploaded()), " to ",
                         base::NumberToString(uploaded)})}
        .SaveTo(tracker_.mutable_status());
    return;
  }
  tracker_.set_uploaded(uploaded);
  tracker_.set_session_token(session_token.data(), session_token.size());
}

void FileUploadJob::Finalize(base::OnceClosure done_cb) {
  base::ScopedClosureRunner done(std::move(done_cb));
  DCHECK_CALLED_ON_VALID_SEQUENCE(job_sequence_checker_);
  if (tracker_.has_status()) {
    // Error detected earlier.
    return;
  }
  if (in_action_) {
    // The job is already executing some action, do nothing.
    return;
  }
  if (tracker_.session_token().empty()) {
    Status{error::FAILED_PRECONDITION, "Job has not been initiated yet"}.SaveTo(
        tracker_.mutable_status());
    return;
  }
  if (tracker_.uploaded() < tracker_.total()) {
    Status{error::DATA_LOSS,
           base::StrCat({"Upload incomplete ",
                         base::NumberToString(tracker_.uploaded()), " out of ",
                         base::NumberToString(tracker_.total())})}
        .SaveTo(tracker_.mutable_status());
    return;
  }
  in_action_ = true;
  if (timer_.IsRunning()) {
    timer_.Reset();
  }

  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&Delegate::DoFinalize, base::Unretained(delegate_),
                     tracker_.session_token(),
                     base::BindPostTaskToCurrentDefault(base::BindOnce(
                         &FileUploadJob::DoneFinalize,
                         weak_ptr_factory_.GetWeakPtr(), std::move(done)))));
}

void FileUploadJob::DoneFinalize(
    base::ScopedClosureRunner done,
    StatusOr<std::string /*access_parameters*/> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(job_sequence_checker_);
  in_action_ = false;
  if (!result.ok()) {
    result.status().SaveTo(tracker_.mutable_status());
    return;
  }
  base::StringPiece access_parameters = result.ValueOrDie();
  if (access_parameters.empty()) {
    Status{error::FAILED_PRECONDITION, "Access parameters not set"}.SaveTo(
        tracker_.mutable_status());
    return;
  }
  tracker_.clear_session_token();
  tracker_.set_access_parameters(access_parameters.data(),
                                 access_parameters.size());
}

const UploadSettings& FileUploadJob::settings() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(job_sequence_checker_);
  return settings_;
}

const UploadTracker& FileUploadJob::tracker() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(job_sequence_checker_);
  return tracker_;
}

base::WeakPtr<FileUploadJob> FileUploadJob::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
}  // namespace reporting
