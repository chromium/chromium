// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/status_uploader.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/syslog_logging.h"
#include "base/system/sys_info.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/status_collector/status_collector.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_request_state.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace em = enterprise_management;

namespace {
// Minimum delay between two consecutive uploads
const int kMinUploadDelayMs = 60 * 1000;  // 60 seconds
// Minimum delay after scheduling an upload
const int kMinUploadScheduleDelayMs = 60 * 1000;  // 60 seconds
// Minimum interval between the last upload and the next immediate upload
constexpr base::TimeDelta kMinImmediateUploadInterval =
    base::TimeDelta::FromSeconds(10);
}  // namespace

namespace policy {

StatusUploader::StatusUploader(
    CloudPolicyClient* client,
    std::unique_ptr<StatusCollector> collector,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    base::TimeDelta default_upload_frequency)
    : client_(client),
      collector_(std::move(collector)),
      task_runner_(task_runner),
      upload_frequency_(default_upload_frequency),
      has_captured_media_(false) {
  // Track whether any media capture devices are in use - this changes what
  // type of information we are allowed to upload.
  MediaCaptureDevicesDispatcher::GetInstance()->AddObserver(this);
  // Listen for changes to the upload delay, and start sending updates to the
  // server.
  upload_frequency_observer_ =
      chromeos::CrosSettings::Get()->AddSettingsObserver(
          chromeos::kReportUploadFrequency,
          base::Bind(&StatusUploader::RefreshUploadFrequency,
                     base::Unretained(this)));

  // Update the upload frequency from settings.
  RefreshUploadFrequency();

  // Schedule our next status upload in a minute (last_upload_ is set to the
  // start of the epoch, so this will trigger an update in
  // kMinUploadScheduleDelayMs from now).
  ScheduleNextStatusUpload();
}

StatusUploader::~StatusUploader() {
  MediaCaptureDevicesDispatcher::GetInstance()->RemoveObserver(this);
}

bool StatusUploader::ScheduleNextStatusUpload(bool immediately) {
  // Don't schedule a new status upload if there's a status upload in progress
  // (it will be scheduled once the current one completes).
  if (status_upload_in_progress_) {
    SYSLOG(INFO) << "In the middle of a status upload, not scheduling the next "
                 << "one until this one finishes.";
    return false;
  }

  base::Time now = base::Time::NowFromSystemTime();

  // Calculate when to fire off the next update (if it should have already
  // happened, this yields a TimeDelta of kMinUploadScheduleDelayMs).
  base::TimeDelta delay =
      std::max((last_upload_ + upload_frequency_) - now,
               base::TimeDelta::FromMilliseconds(kMinUploadScheduleDelayMs));

  // The next upload should be scheduled for at least
  // kMinImmediateUploadInterval after the last upload if it is immediately.
  if (immediately)
    delay = std::max((last_upload_ + kMinImmediateUploadInterval) - now,
                     base::TimeDelta());

  upload_callback_.Reset(base::Bind(&StatusUploader::UploadStatus,
                                    base::Unretained(this)));
  task_runner_->PostDelayedTask(FROM_HERE, upload_callback_.callback(), delay);
  return true;
}

void StatusUploader::RefreshUploadFrequency() {
  // Attempt to fetch the current value of the reporting settings.
  // If trusted values are not available, register this function to be called
  // back when they are available.
  chromeos::CrosSettings* settings = chromeos::CrosSettings::Get();
  if (chromeos::CrosSettingsProvider::TRUSTED != settings->PrepareTrustedValues(
          base::Bind(&StatusUploader::RefreshUploadFrequency,
                     weak_factory_.GetWeakPtr()))) {
    return;
  }

  // CrosSettings are trusted - update our cached upload_frequency (we cache the
  // value because CrosSettings can become untrusted at arbitrary times and we
  // want to use the last trusted value).
  int frequency;
  if (settings->GetInteger(chromeos::kReportUploadFrequency, &frequency)) {
    SYSLOG(INFO) << "Changing status upload frequency from "
                 << upload_frequency_ << " to "
                 << base::TimeDelta::FromMilliseconds(frequency);
    upload_frequency_ = base::TimeDelta::FromMilliseconds(
        std::max(kMinUploadDelayMs, frequency));
  }
  // Schedule a new upload with the new frequency - only do this if we've
  // already performed the initial upload, because we want the initial upload
  // to happen in a minute after startup and not get cancelled by settings
  // changes.
  if (!last_upload_.is_null())
    ScheduleNextStatusUpload();
}

bool StatusUploader::IsSessionDataUploadAllowed() {
  // Check if we're in an auto-launched kiosk session.
  std::unique_ptr<DeviceLocalAccount> account =
      collector_->GetAutoLaunchedKioskSessionInfo();
  if (!account) {
    SYSLOG(WARNING) << "Not a kiosk session, data upload is not allowed.";
    return false;
  }

  // Check if there has been any user input.
  base::TimeTicks last_activity_time =
      ui::UserActivityDetector::Get()->last_activity_time();
  std::string last_activity_name =
      ui::UserActivityDetector::Get()->last_activity_name();
  if (!last_activity_time.is_null()) {
    SYSLOG(WARNING) << "User input " << last_activity_name << " detected "
                    << (base::TimeTicks::Now() - last_activity_time) << " ago ("
                    << (base::SysInfo::Uptime() -
                        (base::TimeTicks::Now() - last_activity_time))
                    << " after last boot), data upload is not allowed.";
    return false;
  }

  // Screenshot is allowed as long as we have not captured media.
  if (has_captured_media_) {
    SYSLOG(WARNING) << "Media has been captured, data upload is not allowed.";
    return false;
  } else {
    return true;
  }
}

void StatusUploader::OnRequestUpdate(int render_process_id,
                                     int render_frame_id,
                                     blink::mojom::MediaStreamType stream_type,
                                     const content::MediaRequestState state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // If a video or audio capture stream is opened, set a flag so we disallow
  // upload of potentially sensitive data.
  if (state == content::MEDIA_REQUEST_STATE_OPENING &&
      (stream_type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE ||
       stream_type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE)) {
    has_captured_media_ = true;
  }
}

bool StatusUploader::ScheduleNextStatusUploadImmediately() {
  return ScheduleNextStatusUpload(true);
}

void StatusUploader::UploadStatus() {
  status_upload_in_progress_ = true;
  // Gather status in the background.
  collector_->GetStatusAsync(base::Bind(&StatusUploader::OnStatusReceived,
                                        weak_factory_.GetWeakPtr()));
}

void StatusUploader::OnStatusReceived(StatusCollectorParams callback_params) {
  bool has_device_status = callback_params.device_status != nullptr;
  bool has_session_status = callback_params.session_status != nullptr;
  bool has_child_status = callback_params.child_status != nullptr;
  if (!has_device_status && !has_session_status && !has_child_status) {
    SYSLOG(INFO) << "Skipping status upload because no data to upload";
    // Don't have any status to upload - just set our timer for next time.
    last_upload_ = base::Time::NowFromSystemTime();
    status_upload_in_progress_ = false;
    ScheduleNextStatusUpload();
    return;
  }
  if (!client_->is_registered()) {
    // This can happen when the DM Token is missing (crbug.com/705607).
    VLOG(1) << "Skipping status upload because the client is not registered";
    // Reset the timer to avoid log spamming.
    last_upload_ = base::Time::NowFromSystemTime();
    status_upload_in_progress_ = false;
    ScheduleNextStatusUpload();
    return;
  }

  SYSLOG(INFO) << "Starting status upload: has_device_status = "
               << has_device_status;

  client_->UploadDeviceStatus(callback_params.device_status.get(),
                              callback_params.session_status.get(),
                              callback_params.child_status.get(),
                              base::Bind(&StatusUploader::OnUploadCompleted,
                                         weak_factory_.GetWeakPtr()));
}

void StatusUploader::OnUploadCompleted(bool success) {
  // Set the last upload time, regardless of whether the upload was successful
  // or not (we don't change the time of the next upload based on whether this
  // upload succeeded or not - if a status upload fails, we just skip it and
  // wait until it's time to try again.
  if (success) {
    SYSLOG(INFO) << "Status upload successful";
  } else {
    SYSLOG(ERROR) << "Error uploading status: " << client_->status();
  }
  last_upload_ = base::Time::NowFromSystemTime();
  status_upload_in_progress_ = false;

  // If the upload was successful, tell the collector so it can clear its cache
  // of pending items.
  if (success)
    collector_->OnSubmittedSuccessfully();

  ScheduleNextStatusUpload();
}

}  // namespace policy
