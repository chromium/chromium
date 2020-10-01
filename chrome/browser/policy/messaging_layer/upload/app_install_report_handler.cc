// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/app_install_report_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/json/json_reader.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/strcat.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/upload/dm_server_upload_service.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/status_macros.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "chrome/browser/policy/messaging_layer/util/task_runner_context.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/proto/record.pb.h"
#include "components/policy/proto/record_constants.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace reporting {

using AppInstallReportUploader =
    AppInstallReportHandler::AppInstallReportUploader;

using UploaderLeaderTracker = AppInstallReportHandler::UploaderLeaderTracker;

// static
scoped_refptr<UploaderLeaderTracker> UploaderLeaderTracker::Create() {
  return base::WrapRefCounted(new UploaderLeaderTracker());
}

StatusOr<AppInstallReportHandler::ReleaseLeaderCallback>
UploaderLeaderTracker::RequestLeaderPromotion() {
  if (has_promoted_app_install_event_uploader_) {
    return Status(error::RESOURCE_EXHAUSTED,
                  "Only one leader is allowed at a time.");
  }

  has_promoted_app_install_event_uploader_ = true;
  return base::BindOnce(&UploaderLeaderTracker::ReleaseLeader,
                        base::Unretained(this));
}

void UploaderLeaderTracker::ReleaseLeader() {
  has_promoted_app_install_event_uploader_ = false;
}

AppInstallReportUploader::AppInstallReportUploader(
    base::Value report,
    scoped_refptr<SharedQueue<base::Value>> report_queue,
    scoped_refptr<UploaderLeaderTracker> leader_tracker,
    policy::CloudPolicyClient* client,
    ClientCallback client_cb,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
    : TaskRunnerContext<bool>(std::move(client_cb), sequenced_task_runner),
      report_(std::move(report)),
      report_queue_(report_queue),
      leader_tracker_(leader_tracker),
      client_(client) {}

AppInstallReportUploader::~AppInstallReportUploader() = default;

void AppInstallReportUploader::OnStart() {
  report_queue_->Push(std::move(report_),
                      base::BindOnce(&AppInstallReportUploader::OnPushComplete,
                                     base::Unretained(this)));
}

void AppInstallReportUploader::OnPushComplete() {
  Schedule(&AppInstallReportUploader::RequestLeaderPromotion,
           base::Unretained(this));
}

void AppInstallReportUploader::RequestLeaderPromotion() {
  auto promo_result = leader_tracker_->RequestLeaderPromotion();
  if (!promo_result.ok()) {
    Complete();
    return;
  }

  release_leader_cb_ = std::move(promo_result.ValueOrDie());

  ScheduleNextPop();
}

void AppInstallReportUploader::ScheduleNextPop() {
  report_queue_->Pop(base::BindOnce(&AppInstallReportUploader::OnPopResult,
                                    base::Unretained(this)));
}

void AppInstallReportUploader::OnPopResult(StatusOr<base::Value> pop_result) {
  if (!pop_result.ok()) {
    // There are no more records to process - exit.
    std::move(release_leader_cb_).Run();
    Complete();
    return;
  }

  Schedule(&AppInstallReportUploader::StartUpload, base::Unretained(this),
           std::move(pop_result.ValueOrDie()));
}

void AppInstallReportUploader::StartUpload(base::Value record) {
  ClientCallback cb = base::BindOnce(
      &AppInstallReportUploader::OnUploadComplete, base::Unretained(this));
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(
                     [](policy::CloudPolicyClient* client, base::Value record,
                        ClientCallback cb) {
                       client->UploadExtensionInstallReport(std::move(record),
                                                            std::move(cb));
                     },
                     client_, std::move(record), std::move(cb)));
}

void AppInstallReportUploader::OnUploadComplete(bool success) {
  if (!success) {
    LOG(ERROR) << Status(error::DATA_LOSS, "Upload was unsuccessful");
  }

  Schedule(&AppInstallReportUploader::ScheduleNextPop, base::Unretained(this));
}

void AppInstallReportUploader::Complete() {
  Schedule(&AppInstallReportUploader::Response, base::Unretained(this), true);
}

AppInstallReportHandler::AppInstallReportHandler(
    policy::CloudPolicyClient* client)
    : RecordHandler(client),
      report_queue_(SharedQueue<base::Value>::Create()),
      leader_tracker_(UploaderLeaderTracker::Create()),
      sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})) {}

AppInstallReportHandler::~AppInstallReportHandler() = default;

Status AppInstallReportHandler::HandleRecord(Record record) {
  RETURN_IF_ERROR(ValidateRecord(record));
  ASSIGN_OR_RETURN(base::Value report, ConvertRecord(record));

  ClientCallback client_cb = base::BindOnce([](bool finished_running) {
    VLOG(1) << "Finished Running AppInstallReportUploader";
  });

  // Start an uploader in case any previous uploader has finished running before
  // this record was posted.
  Start<AppInstallReportUploader>(std::move(report), report_queue_,
                                  leader_tracker_, GetClient(),
                                  std::move(client_cb), sequenced_task_runner_);

  return Status::StatusOK();
}

Status AppInstallReportHandler::ValidateRecord(const Record& record) const {
  return ValidateDestination(record, Destination::APP_INSTALL_EVENT);
}

Status AppInstallReportHandler::ValidateDestination(
    const Record& record,
    Destination expected_destination) const {
  if (record.destination() != expected_destination) {
    return Status(
        error::INVALID_ARGUMENT,
        base::StrCat({"Record destination mismatch, expected=",
                      Destination_Name(expected_destination), ", encountered=",
                      Destination_Name(record.destination())}));
  }
  return Status::StatusOK();
}

StatusOr<base::Value> AppInstallReportHandler::ConvertRecord(
    const Record& record) const {
  base::Optional<base::Value> report_result =
      base::JSONReader::Read(record.data());
  if (!report_result.has_value()) {
    return Status(error::INVALID_ARGUMENT, "Unknown Report Format");
  }

  return std::move(report_result.value());
}

Status AppInstallReportHandler::ValidateClientState() const {
  if (!GetClient()->is_registered()) {
    return Status(error::UNAVAILABLE, "DmServer is currently unavailable");
  }
  return Status::StatusOK();
}

}  // namespace reporting
