// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/arc_app_install_event_log_uploader.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/reporting/install_event_log_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

ArcAppInstallEventLogUploader::Delegate::~Delegate() {}

ArcAppInstallEventLogUploader::ArcAppInstallEventLogUploader(
    CloudPolicyClient* client,
    Profile* profile)
    : InstallEventLogUploaderBase(client, profile) {}

ArcAppInstallEventLogUploader::~ArcAppInstallEventLogUploader() {
  CancelClientUpload();
}

void ArcAppInstallEventLogUploader::SetDelegate(Delegate* delegate) {
  if (delegate_) {
    CancelUpload();
  }
  delegate_ = delegate;
}

void ArcAppInstallEventLogUploader::CancelClientUpload() {
  weak_factory_.InvalidateWeakPtrs();
  client_->CancelAppInstallReportUpload();
}

void ArcAppInstallEventLogUploader::StartSerialization() {
  delegate_->SerializeForUpload(
      base::BindOnce(&ArcAppInstallEventLogUploader::OnSerialized,
                     weak_factory_.GetWeakPtr()));
}

void ArcAppInstallEventLogUploader::CheckDelegateSet() {
  CHECK(delegate_);
}

void ArcAppInstallEventLogUploader::OnUploadSuccess() {
  delegate_->OnUploadSuccess();
}

void ArcAppInstallEventLogUploader::PostTaskForStartSerialization() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&InstallEventLogUploaderBase::StartSerialization,
                     weak_factory_.GetWeakPtr()),
      base::Milliseconds(retry_backoff_ms_));
}

void ArcAppInstallEventLogUploader::OnSerialized(
    const em::AppInstallReportRequest* report) {
  base::Value::Dict context = reporting::GetContext(profile_);
  base::Value::List event_list = ConvertArcAppProtoToValue(report, context);

  base::Value::Dict value_report =
      RealtimeReportingJobConfiguration::BuildReport(std::move(event_list),
                                                     std::move(context));

  // base::Unretained() is safe here as the destructor cancels any pending
  // upload, after which the |client_| is guaranteed to not call the callback.
  client_->UploadAppInstallReport(
      std::move(value_report),
      base::BindOnce(&ArcAppInstallEventLogUploader::OnUploadDone,
                     base::Unretained(this)));
}

}  // namespace policy
