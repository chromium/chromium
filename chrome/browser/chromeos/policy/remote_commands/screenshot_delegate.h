// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_SCREENSHOT_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_SCREENSHOT_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/policy/remote_commands/device_command_screenshot_job.h"
#include "chrome/browser/chromeos/policy/upload_job.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/snapshot/snapshot.h"
#include "url/gurl.h"

namespace policy {

// An implementation of the |DeviceCommandScreenshotJob::Delegate| that uses
// aura's GrabWindowSnapshotAsyncPNG() to acquire the window snapshot.
class ScreenshotDelegate : public DeviceCommandScreenshotJob::Delegate {
 public:
  ScreenshotDelegate();
  ~ScreenshotDelegate() override;

  // DeviceCommandScreenshotJob::Delegate:
  bool IsScreenshotAllowed() override;
  void TakeSnapshot(gfx::NativeWindow window,
                    const gfx::Rect& source_rect,
                    ui::GrabWindowSnapshotAsyncPNGCallback callback) override;
  std::unique_ptr<UploadJob> CreateUploadJob(
      const GURL& upload_url,
      UploadJob::Delegate* delegate) override;

 private:
  void StoreScreenshot(ui::GrabWindowSnapshotAsyncPNGCallback callback,
                       scoped_refptr<base::RefCountedMemory> png_data);

  base::WeakPtrFactory<ScreenshotDelegate> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ScreenshotDelegate);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_SCREENSHOT_DELEGATE_H_
