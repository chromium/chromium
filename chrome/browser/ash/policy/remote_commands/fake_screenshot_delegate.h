// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_FAKE_SCREENSHOT_DELEGATE_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_FAKE_SCREENSHOT_DELEGATE_H_

#include "chrome/browser/ash/policy/remote_commands/device_command_screenshot_job.h"

namespace policy {

class FakeScreenshotDelegate
    : public policy::DeviceCommandScreenshotJob::Delegate {
 public:
  FakeScreenshotDelegate() = default;
  ~FakeScreenshotDelegate() override = default;
  FakeScreenshotDelegate(const FakeScreenshotDelegate&) = delete;
  FakeScreenshotDelegate& operator=(const FakeScreenshotDelegate&) = delete;

  bool IsScreenshotAllowed() override;

  void TakeSnapshot(gfx::NativeWindow window,
                    const gfx::Rect& source_rect,
                    policy::OnScreenshotTakenCallback callback) override;

  std::unique_ptr<policy::UploadJob> CreateUploadJob(
      const GURL&,
      policy::UploadJob::Delegate*) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_FAKE_SCREENSHOT_DELEGATE_H_
