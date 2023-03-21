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
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/browser/policy/messaging_layer/public/report_client.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/proto/synced/upload_tracker.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/storage/storage_module_interface.h"
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
    Priority priority,
    Record record_copy,
    ::ash::reporting::LogUploadEvent log_upload_event,
    Delegate* delegate,
    base::OnceCallback<void(StatusOr<FileUploadJob*>)> result_cb) {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](Manager* self, Priority priority, Record record_copy,
             ::ash::reporting::LogUploadEvent log_upload_event,
             Delegate* delegate,
             base::OnceCallback<void(StatusOr<FileUploadJob*>)> result_cb) {
            // Serialize settings to get the map key.
            std::string serialized_settings;
            if (!log_upload_event.upload_settings().SerializeToString(
                    &serialized_settings)) {
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
                  std::make_unique<FileUploadJob>(
                      log_upload_event.upload_settings(),
                      log_upload_event.upload_tracker(), delegate));
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
            auto* job = it->second.get();
            // Check the `job` state, schedule the action.
            DCHECK_CALLED_ON_VALID_SEQUENCE(job->job_sequence_checker_);
            if (job->event_helper_) {
              // The job already executes, the event we are dealing with
              // is likely the one that caused this, do not upload it
              // (otherwise we would lose track of the job if the device
              // restarts).
              std::move(result_cb).Run(
                  Status(error::ALREADY_EXISTS, "Duplicate event"));
              return;
            }
            // Attach the event to the job.
            job->event_helper_ = std::make_unique<FileUploadJob::EventHelper>(
                job->GetWeakPtr(), priority, std::move(record_copy),
                std::move(log_upload_event));
            std::move(result_cb).Run(job);
          },
          base::Unretained(this), priority, std::move(record_copy),
          std::move(log_upload_event), base::Unretained(delegate),
          std::move(result_cb)));
}

scoped_refptr<base::SequencedTaskRunner>
FileUploadJob::Manager::sequenced_task_runner() const {
  return sequenced_task_runner_;
}

// EventHelper implementation.

FileUploadJob::EventHelper::EventHelper(
    base::WeakPtr<FileUploadJob> job,
    Priority priority,
    Record record_copy,
    ::ash::reporting::LogUploadEvent log_upload_event)
    : job_(job),
      priority_(priority),
      record_copy_(std::move(record_copy)),
      log_upload_event_(std::move(log_upload_event)) {}

FileUploadJob::EventHelper::~EventHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (done_cb_) {
    Complete(Status(error::DATA_LOSS,
                    "Helper started but completion callback not called."));
  }
}

void FileUploadJob::EventHelper::Run(
    const ScopedReservation& scoped_reservation,
    base::OnceCallback<void(Status)> done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!done_cb_) << "Helper already running";
  done_cb_ = std::move(done_cb);
  if (job_->tracker().has_status()) {
    // The job already failed before. Upload the event as is.
    Complete();
    return;
  }
  if (!job_->tracker().access_parameters().empty()) {
    // Job complete, nothing left to do. Upload the event as is.
    Complete();
    return;
  }
  if (job_->tracker().session_token().empty()) {
    // Job not initiated yet, do it now. Upon completion success post new
    // event and upload the current one.
    job_->Initiate(base::BindOnce(&EventHelper::RepostAndComplete,
                                  weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  if (log_upload_event_.upload_tracker().session_token().empty()) {
    // Event refers to the job_ before it was initiated.
    // Upload the event, do not post a new one.
    Complete();
    return;
  }
  // Job in progress check what was uploaded.
  if (job_->tracker().uploaded() >
      log_upload_event_.upload_tracker().uploaded()) {
    // The `job_` is more advanced than the event implies.
    // Upload the event, do not post a new one.
    Complete();
    return;
  }
  if (job_->tracker().uploaded() <
      log_upload_event_.upload_tracker().uploaded()) {
    // The `job_` is less advanced than the event implies, it should not be
    // possible unless the `job_` is corrupt.
    LOG(WARNING) << "Corrupt FileUploadJob";
    // Upload the event, do not post a new one.
    Complete();
    return;
  }
  // Exact match, resume the `job_`. Note that if the `job_` is already
  // active, this will be a no-op.
  if (job_->tracker().uploaded() < job_->tracker().total()) {
    // Job in progress, perform next step. Upon completion success post new
    // event and upload the current one.
    job_->NextStep(scoped_reservation,
                   base::BindOnce(&EventHelper::RepostAndComplete,
                                  weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  // Upload complete, finalize the job_.
  job_->Finalize(base::BindOnce(&EventHelper::RepostAndComplete,
                                weak_ptr_factory_.GetWeakPtr()));
}

void FileUploadJob::EventHelper::Complete(Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(done_cb_);
  std::move(done_cb_).Run(status);
  // Disconnect from the job, self destruct.
  DCHECK_CALLED_ON_VALID_SEQUENCE(job_->job_sequence_checker_);
  job_->event_helper_.reset();
}

void FileUploadJob::EventHelper::RepostAndComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Post a new event reflecting its state to track later.
  // If `job_` is not available, do not allow to upload the current event.
  if (!job_) {
    Complete(Status(error::DATA_LOSS, "Upload Job has been removed"));
    return;
  }
  // Job is still around. Update the new event with its status.
  if (job_->tracker().access_parameters().empty() &&
      !job_->tracker().has_status()) {
    // The job_ is in progress (not succeeded and not failed),
    // flag the new tracking event to be processed when reaching uploader.
    record_copy_.set_needs_local_unencrypted_copy(true);
  }
  // Copy it tracking state to the new event.
  *log_upload_event_.mutable_upload_settings() = job_->settings();
  *log_upload_event_.mutable_upload_tracker() = job_->tracker();
  // Patch the copy event.
  if (!log_upload_event_.SerializeToString(record_copy_.mutable_data())) {
    Complete(Status(error::INVALID_ARGUMENT,
                    base::StrCat({"Updated event ",
                                  Destination_Name(record_copy_.destination()),
                                  " failed to serialize"})));
    return;
  }
  // Repost the copy event and return result via `Complete`.
  FileUploadJob::AddRecordToStorage(
      priority_, std::move(record_copy_),
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &EventHelper::Complete, weak_ptr_factory_.GetWeakPtr())));
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
  DCHECK(event_helper_) << "Event must be associated with the job";
  if (tracker_.has_status()) {
    // Error detected earlier.
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
  DCHECK(event_helper_) << "Event must be associated with the job";
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

void FileUploadJob::NextStep(const ScopedReservation& scoped_reservation,
                             base::OnceClosure done_cb) {
  base::ScopedClosureRunner done(std::move(done_cb));
  DCHECK_CALLED_ON_VALID_SEQUENCE(job_sequence_checker_);
  DCHECK(event_helper_) << "Event must be associated with the job";
  if (tracker_.has_status()) {
    // Error detected earlier.
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
  if (timer_.IsRunning()) {
    timer_.Reset();
  }
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&Delegate::DoNextStep, base::Unretained(delegate_),
                     tracker_.total(), tracker_.uploaded(),
                     tracker_.session_token(),
                     ScopedReservation(0uL, scoped_reservation),
                     base::BindPostTaskToCurrentDefault(base::BindOnce(
                         &FileUploadJob::DoneNextStep,
                         weak_ptr_factory_.GetWeakPtr(), std::move(done)))));
}

void FileUploadJob::DoneNextStep(
    base::ScopedClosureRunner done,
    StatusOr<std::pair<int64_t /*uploaded*/, std::string /*session_token*/>>
        result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(job_sequence_checker_);
  DCHECK(event_helper_) << "Event must be associated with the job";
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
  DCHECK(event_helper_) << "Event must be associated with the job";
  if (tracker_.has_status()) {
    // Error detected earlier.
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
  DCHECK(event_helper_) << "Event must be associated with the job";
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

// static
void FileUploadJob::AddRecordToStorage(
    Priority priority,
    Record record_copy,
    base::OnceCallback<void(Status)> done_cb) {
  ReportingClient::GetInstance()->sequenced_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](Priority priority, Record record_copy,
                        base::OnceCallback<void(Status)> done_cb) {
                       // We can only get to here from upload, which originates
                       // from Storage Module, so `storage()` below cannot be
                       // null.
                       DCHECK(ReportingClient::GetInstance()->storage());
                       ReportingClient::GetInstance()->storage()->AddRecord(
                           priority, std::move(record_copy),
                           std::move(done_cb));
                     },
                     priority, std::move(record_copy), std::move(done_cb)));
}

void FileUploadJob::SetEventHelperForTest(
    std::unique_ptr<FileUploadJob::EventHelper> event_helper) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(job_sequence_checker_);
  event_helper_ = std::move(event_helper);
}

FileUploadJob::EventHelper* FileUploadJob::event_helper() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(job_sequence_checker_);
  return event_helper_.get();
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
