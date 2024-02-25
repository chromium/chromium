// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_UPLOADING_STATUS_UPLOADER_H_
#define CHROME_BROWSER_ASH_POLICY_UPLOADING_STATUS_UPLOADER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/cancelable_callback.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class StatusCollector;
struct StatusCollectorParams;

// Class responsible for periodically uploading device status from the
// passed StatusCollector.
class StatusUploader : public MediaCaptureDevicesDispatcher::Observer {
 public:
  // Constructor. |client| must be registered and must stay
  // valid and registered through the lifetime of this StatusUploader
  // object.
  StatusUploader(CloudPolicyClient* client,
                 std::unique_ptr<StatusCollector> collector,
                 const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                 base::TimeDelta default_upload_frequency);

  StatusUploader(const StatusUploader&) = delete;
  StatusUploader& operator=(const StatusUploader&) = delete;

  ~StatusUploader() override;

  // Returns the time of the last successful upload, or Time(0) if no upload
  // has ever happened.
  base::Time last_upload() const { return last_upload_; }

  // Returns true if screenshot upload is allowed. This checks to ensure that
  // the current session is a kiosk session and that no user input (keyboard,
  // mouse, touch) has been received in the last 5 minutes. If there has been
  // audio/video captured in this session it will be blocked till reboot.
  bool IsScreenshotAllowed();

  // MediaCaptureDevicesDispatcher::Observer implementation
  void OnRequestUpdate(int render_process_id,
                       int render_frame_id,
                       blink::mojom::MediaStreamType stream_type,
                       const content::MediaRequestState state) override;

  // Returns true if the next status upload has been scheduled successfully.
  // Returns false if there is already an ongoing status report.
  bool ScheduleNextStatusUploadImmediately();

  StatusCollector* status_collector() const { return collector_.get(); }

 private:
  // Callback invoked periodically to upload the device status from the
  // StatusCollector.
  void UploadStatus();

  // Called asynchronously by StatusCollector when status arrives.
  void OnStatusReceived(StatusCollectorParams callback_params);

  // Invoked once a status upload has completed.
  void OnUploadCompleted(CloudPolicyClient::Result result);

  // Helper method that figures out when the next status upload should
  // be scheduled. Returns true if the next status upload has been scheduled
  // successfully, returns false if there is already an ongoing status report.
  bool ScheduleNextStatusUpload(bool immediately = false);

  // Updates the upload frequency from settings and schedules a new upload
  // if appropriate.
  void RefreshUploadFrequency();

  // CloudPolicyClient used to issue requests to the server.
  raw_ptr<CloudPolicyClient> client_;

  // StatusCollector that provides status for uploading.
  std::unique_ptr<StatusCollector> collector_;

  // TaskRunner used for scheduling upload tasks.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // How long to wait between status uploads.
  base::TimeDelta upload_frequency_;

  // Subscription for the callback about changes in the upload frequency.
  base::CallbackListSubscription upload_frequency_subscription_;

  // The time the last upload was performed.
  base::Time last_upload_;

  // Callback invoked via a delay to upload device status.
  base::CancelableOnceClosure upload_callback_;

  // True if there has been any captured media in this session.
  bool has_captured_media_;

  // Used to prevent a race condition where two status uploads are being
  // executed in parallel.
  bool status_upload_in_progress_ = false;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<StatusUploader> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_UPLOADING_STATUS_UPLOADER_H_
