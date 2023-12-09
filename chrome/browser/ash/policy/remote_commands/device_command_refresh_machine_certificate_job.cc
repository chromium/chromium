// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_refresh_machine_certificate_job.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/syslog_logging.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/attestation/machine_certificate_uploader.h"

namespace policy {

namespace {

// This command has an expiration time this high with the same reasons as for
// `DeviceCommandWipeUsersJob::kWipeUsersCommandExpirationTime`.
constexpr base::TimeDelta kRefreshMachineCertificateCommandExpirationTime =
    base::Days(180);

}  // namespace

DeviceCommandRefreshMachineCertificateJob::
    DeviceCommandRefreshMachineCertificateJob(
        ash::attestation::MachineCertificateUploader*
            machine_certificate_uploader)
    : machine_certificate_uploader_(machine_certificate_uploader) {}

DeviceCommandRefreshMachineCertificateJob::
    ~DeviceCommandRefreshMachineCertificateJob() = default;

enterprise_management::RemoteCommand_Type
DeviceCommandRefreshMachineCertificateJob::GetType() const {
  return enterprise_management::
      RemoteCommand_Type_DEVICE_REFRESH_ENTERPRISE_MACHINE_CERTIFICATE;
}

bool DeviceCommandRefreshMachineCertificateJob::IsExpired(base::TimeTicks now) {
  return now > issued_time() + kRefreshMachineCertificateCommandExpirationTime;
}

void DeviceCommandRefreshMachineCertificateJob::RunImpl(
    CallbackWithResult result_callback) {
  if (machine_certificate_uploader_) {
    SYSLOG(INFO) << "Refreshing enterprise machine certificate.";
    machine_certificate_uploader_->RefreshAndUploadCertificate(base::BindOnce(
        &DeviceCommandRefreshMachineCertificateJob::OnCertificateUploaded,
        weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
  } else {
    SYSLOG(WARNING) << "Machine certificate uploader unavailable,"
                    << " certificate cannot be refreshed.";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_callback),
                                  ResultType::kFailure, std::nullopt));
  }
}

void DeviceCommandRefreshMachineCertificateJob::OnCertificateUploaded(
    CallbackWithResult result_callback,
    bool success) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(result_callback),
                     success ? ResultType::kSuccess : ResultType::kFailure,
                     std::nullopt));
}

}  // namespace policy
