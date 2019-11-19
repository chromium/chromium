// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/device_command_refresh_machine_certificate_job.h"

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/syslog_logging.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/attestation/machine_certificate_uploader.h"

namespace policy {

DeviceCommandRefreshMachineCertificateJob::
    DeviceCommandRefreshMachineCertificateJob(
        chromeos::attestation::MachineCertificateUploader*
            machine_certificate_uploader)
    : machine_certificate_uploader_(machine_certificate_uploader) {}

DeviceCommandRefreshMachineCertificateJob::
    ~DeviceCommandRefreshMachineCertificateJob() = default;

enterprise_management::RemoteCommand_Type
DeviceCommandRefreshMachineCertificateJob::GetType() const {
  return enterprise_management::
      RemoteCommand_Type_DEVICE_REFRESH_ENTERPRISE_MACHINE_CERTIFICATE;
}

void DeviceCommandRefreshMachineCertificateJob::RunImpl(
    CallbackWithResult succeeded_callback,
    CallbackWithResult failed_callback) {
  if (machine_certificate_uploader_) {
    SYSLOG(INFO) << "Refreshing enterprise machine certificate.";
    machine_certificate_uploader_->RefreshAndUploadCertificate(base::BindOnce(
        &DeviceCommandRefreshMachineCertificateJob::OnCertificateUploaded,
        weak_ptr_factory_.GetWeakPtr(), std::move(succeeded_callback),
        std::move(failed_callback)));
  } else {
    SYSLOG(WARNING) << "Machine certificate uploader unavailable,"
                    << " certificate cannot be refreshed.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(failed_callback), nullptr));
  }
}

void DeviceCommandRefreshMachineCertificateJob::OnCertificateUploaded(
    CallbackWithResult succeeded_callback,
    CallbackWithResult failed_callback,
    bool success) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(success ? succeeded_callback : failed_callback),
                     nullptr));
}

}  // namespace policy
