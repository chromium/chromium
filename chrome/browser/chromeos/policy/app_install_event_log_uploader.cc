// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/app_install_event_log_uploader.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/app_install_event_log_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// The backoff time starts at |kMinRetryBackoffMs| and doubles after each upload
// failure until it reaches |kMaxRetryBackoffMs|, from which point on it remains
// constant. The backoff is reset to |kMinRetryBackoffMs| after the next
// successful upload or if the upload request is canceled.
const int kMinRetryBackoffMs = 10 * 1000;            // 10 seconds
const int kMaxRetryBackoffMs = 24 * 60 * 60 * 1000;  // 24 hours

}  // namespace

AppInstallEventLogUploader::Delegate::~Delegate() {}

AppInstallEventLogUploader::AppInstallEventLogUploader(
    CloudPolicyClient* client,
    Profile* profile)
    : client_(client),
      profile_(profile),
      retry_backoff_ms_(kMinRetryBackoffMs) {
  DCHECK(client_);
  client_->AddObserver(this);
}

AppInstallEventLogUploader::~AppInstallEventLogUploader() {
  CancelUpload();
  client_->RemoveObserver(this);
}

void AppInstallEventLogUploader::SetDelegate(Delegate* delegate) {
  if (delegate_) {
    CancelUpload();
  }
  delegate_ = delegate;
}

void AppInstallEventLogUploader::RequestUpload() {
  CHECK(delegate_);
  if (upload_requested_) {
    return;
  }

  upload_requested_ = true;
  if (client_->is_registered()) {
    StartSerialization();
  }
}

void AppInstallEventLogUploader::CancelUpload() {
  weak_factory_.InvalidateWeakPtrs();
  client_->CancelAppInstallReportUpload();
  upload_requested_ = false;
  retry_backoff_ms_ = kMinRetryBackoffMs;
}

void AppInstallEventLogUploader::OnRegistrationStateChanged(
    CloudPolicyClient* client) {
  if (!upload_requested_) {
    return;
  }

  if (client->is_registered()) {
    StartSerialization();
  } else {
    CancelUpload();
    RequestUpload();
  }
}

void AppInstallEventLogUploader::StartSerialization() {
  delegate_->SerializeForUpload(base::BindOnce(
      &AppInstallEventLogUploader::OnSerialized, weak_factory_.GetWeakPtr()));
}

void AppInstallEventLogUploader::OnSerialized(
    const em::AppInstallReportRequest* report) {
  base::Value event_list = ConvertProtoToValue(report, profile_);
  base::Value context = reporting::GetContext(profile_);
  AppendEventId(&event_list, context);

  base::Value value_report = RealtimeReportingJobConfiguration::BuildReport(
      std::move(event_list), std::move(context));

  // base::Unretained() is safe here as the destructor cancels any pending
  // upload, after which the |client_| is guaranteed to not call the callback.
  client_->UploadRealtimeReport(
      std::move(value_report),
      base::AdaptCallbackForRepeating(base::BindOnce(
          &AppInstallEventLogUploader::OnUploadDone, base::Unretained(this))));
}

void AppInstallEventLogUploader::OnUploadDone(bool success) {
  if (success) {
    upload_requested_ = false;
    retry_backoff_ms_ = kMinRetryBackoffMs;
    delegate_->OnUploadSuccess();
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AppInstallEventLogUploader::StartSerialization,
                     weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(retry_backoff_ms_));
  retry_backoff_ms_ = std::min(retry_backoff_ms_ << 1, kMaxRetryBackoffMs);
}

}  // namespace policy
