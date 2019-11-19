// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_SCREENSHOT_JOB_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_SCREENSHOT_JOB_H_

#include <stddef.h>

#include <list>
#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/task_runner.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/policy/upload_job.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/snapshot/snapshot.h"
#include "url/gurl.h"

namespace policy {

// This class implementens a RemoteCommandJob that captures a screenshot from
// each attached screen and uploads the collected PNG data using UploadJob to
// the url specified in the "fileUploadUrl" field of the CommandPayload. Only
// one instance of this command will be running at a time. The
// RemoteCommandsQueue owns all instances of this class.
class DeviceCommandScreenshotJob : public RemoteCommandJob,
                                   public UploadJob::Delegate {
 public:
  // When the screenshot command terminates, the result payload that gets sent
  // to the server is populated with one of the following result codes. These
  // are exposed publicly here since DeviceCommandScreenshotTest uses them.
  enum ResultCode {
    // Successfully uploaded screenshots.
    SUCCESS = 0,

    // Aborted screenshot acquisition due to user input having been entered.
    FAILURE_USER_INPUT = 1,

    // Failed to acquire screenshots, e.g. no attached screens.
    FAILURE_SCREENSHOT_ACQUISITION = 2,

    // Failed to authenticate to the remote server.
    FAILURE_AUTHENTICATION = 3,

    // Failed due to an internal server error.
    FAILURE_SERVER = 4,

    // Failed due to a client-side error.
    FAILURE_CLIENT = 5,

    // Failed due to an invalid upload url.
    FAILURE_INVALID_URL = 6
  };

  // A delegate interface used by DeviceCommandScreenshotJob to retrieve its
  // dependencies.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Returns true if screenshots are allowed in this session. Returns false
    // if the current session is not an auto-launched kiosk session, or there
    // have been certain types of user input that may result in leaking private
    // information.
    virtual bool IsScreenshotAllowed() = 0;

    // Acquires a snapshot of |source_rect| in |window| and invokes |callback|
    // with the PNG data. The passed-in callback will not be invoked after the
    // delegate has been destroyed. See e.g. ScreenshotDelegate.
    virtual void TakeSnapshot(
        gfx::NativeWindow window,
        const gfx::Rect& source_rect,
        ui::GrabWindowSnapshotAsyncPNGCallback callback) = 0;

    // Creates a new fully configured instance of an UploadJob. This method
    // may be called any number of times.
    virtual std::unique_ptr<UploadJob> CreateUploadJob(
        const GURL&,
        UploadJob::Delegate*) = 0;
  };

  explicit DeviceCommandScreenshotJob(
      std::unique_ptr<Delegate> screenshot_delegate);
  ~DeviceCommandScreenshotJob() override;

  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;

 private:
  class Payload;

  // UploadJob::Delegate:
  void OnSuccess() override;
  void OnFailure(UploadJob::ErrorCode error_code) override;

  // RemoteCommandJob:
  bool ParseCommandPayload(const std::string& command_payload) override;
  void RunImpl(CallbackWithResult succeeded_callback,
               CallbackWithResult failed_callback) override;
  void TerminateImpl() override;

  void StoreScreenshot(size_t screen,
                       scoped_refptr<base::RefCountedMemory> png_data);

  void StartScreenshotUpload();

  // The URL to which the POST request should be directed.
  GURL upload_url_;

  // The callback that will be called when the screenshot was successfully
  // uploaded.
  CallbackWithResult succeeded_callback_;

  // The callback that will be called when this command failed.
  CallbackWithResult failed_callback_;

  // Tracks the number of pending screenshots.
  int num_pending_screenshots_;

  // Caches the already completed screenshots for the different displays.
  std::map<int, scoped_refptr<base::RefCountedMemory>> screenshots_;

  // The Delegate is used to acquire screenshots and create UploadJobs.
  std::unique_ptr<Delegate> screenshot_delegate_;

  // The upload job instance that will upload the screenshots.
  std::unique_ptr<UploadJob> upload_job_;

  base::WeakPtrFactory<DeviceCommandScreenshotJob> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeviceCommandScreenshotJob);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_SCREENSHOT_JOB_H_
