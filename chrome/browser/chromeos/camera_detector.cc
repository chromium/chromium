// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/camera_detector.h"

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "components/storage_monitor/udev_util_linux.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

namespace {

// Sysfs directory containing V4L devices.
const char kV4LSubsystemDir[] = "/sys/class/video4linux/";
// Name of the udev property with V4L capabilities.
const char kV4LCapabilities[] = "ID_V4L_CAPABILITIES";
// Delimiter character for udev V4L capabilities.
const char kV4LCapabilitiesDelim = ':';
// V4L capability that denotes a capture-enabled device.
const char kV4LCaptureCapability[] = "capture";

}  // namespace

using content::BrowserThread;

// static
CameraDetector::CameraPresence CameraDetector::camera_presence_ =
    CameraDetector::kCameraPresenceUnknown;

// static
bool CameraDetector::presence_check_in_progress_ = false;

// static
void CameraDetector::StartPresenceCheck(const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (presence_check_in_progress_)
    return;
  DVLOG(1) << "Starting camera presence check";
  presence_check_in_progress_ = true;
  base::PostTaskAndReplyWithResult(
      base::CreateTaskRunner({base::ThreadPool(), base::MayBlock(),
                              base::TaskPriority::BEST_EFFORT,
                              base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})
          .get(),
      FROM_HERE, base::Bind(&CameraDetector::CheckPresence),
      base::Bind(&CameraDetector::OnPresenceCheckDone, callback));
}

// static
void CameraDetector::OnPresenceCheckDone(const base::Closure& callback,
                                         bool present) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  camera_presence_ = present ? kCameraPresent : kCameraAbsent;
  presence_check_in_progress_ = false;
  callback.Run();
}

// static
bool CameraDetector::CheckPresence() {
  // We do a quick check using udev database because opening each /dev/videoX
  // device may trigger costly device initialization.
  base::FileEnumerator file_enum(
      base::FilePath(kV4LSubsystemDir), false /* not recursive */,
      base::FileEnumerator::FILES | base::FileEnumerator::SHOW_SYM_LINKS);
  for (base::FilePath path = file_enum.Next(); !path.empty();
       path = file_enum.Next()) {
    std::string v4l_capabilities;
    if (storage_monitor::GetUdevDevicePropertyValueByPath(
            path, kV4LCapabilities, &v4l_capabilities)) {
      std::vector<std::string> caps = base::SplitString(
          v4l_capabilities, std::string(1, kV4LCapabilitiesDelim),
          base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
      if (find(caps.begin(), caps.end(), kV4LCaptureCapability) != caps.end()) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace chromeos
