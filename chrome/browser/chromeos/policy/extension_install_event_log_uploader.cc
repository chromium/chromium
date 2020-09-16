// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/extension_install_event_log_uploader.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/install_event_log_util.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

ExtensionInstallEventLogUploader::Delegate::~Delegate() {}

ExtensionInstallEventLogUploader::ExtensionInstallEventLogUploader(
    CloudPolicyClient* client,
    Profile* profile)
    : InstallEventLogUploaderBase(client, profile) {}

ExtensionInstallEventLogUploader::~ExtensionInstallEventLogUploader() {
  CancelClientUpload();
}

void ExtensionInstallEventLogUploader::SetDelegate(Delegate* delegate) {
  if (delegate_)
    CancelUpload();
  delegate_ = delegate;
}

void ExtensionInstallEventLogUploader::CancelClientUpload() {
  weak_factory_.InvalidateWeakPtrs();
  client_->CancelExtensionInstallReportUpload();
}

void ExtensionInstallEventLogUploader::StartSerialization() {
  delegate_->SerializeExtensionLogForUpload(
      base::BindOnce(&ExtensionInstallEventLogUploader::OnSerialized,
                     weak_factory_.GetWeakPtr()));
}

void ExtensionInstallEventLogUploader::CheckDelegateSet() {
  CHECK(delegate_);
}

void ExtensionInstallEventLogUploader::OnUploadSuccess() {
  delegate_->OnExtensionLogUploadSuccess();
}

void ExtensionInstallEventLogUploader::PostTaskForStartSerialization() {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExtensionInstallEventLogUploader::StartSerialization,
                     weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(retry_backoff_ms_));
}

void ExtensionInstallEventLogUploader::OnSerialized(
    const em::ExtensionInstallReportRequest* report) {
  base::Value context = reporting::GetContext(profile_);
  base::Value event_list = ConvertExtensionProtoToValue(report, context);

  base::Value value_report = RealtimeReportingJobConfiguration::BuildReport(
      std::move(event_list), std::move(context));

  // base::Unretained() is safe here as the destructor cancels any pending
  // upload, after which the |client_| is guaranteed to not call the callback.
  client_->UploadExtensionInstallReport(
      std::move(value_report),
      base::BindOnce(&ExtensionInstallEventLogUploader::OnUploadDone,
                     base::Unretained(this)));
}

}  // namespace policy
