// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_SIGNOUT_SCREENSHOT_HANDLER_H_
#define ASH_GLANCEABLES_SIGNOUT_SCREENSHOT_HANDLER_H_

#include "ash/ash_export.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Image;
}  // namespace gfx

namespace ash {

// Handles taking a screenshot of the open windows on signout or shutdown.
// Encodes the screenshot as PNG and writes it to the user data directory.
// Invokes a callback when done so that signout can proceed. The screenshot is
// displayed by the glanceables screen on the next login.
class ASH_EXPORT SignoutScreenshotHandler {
 public:
  SignoutScreenshotHandler();
  SignoutScreenshotHandler(const SignoutScreenshotHandler&) = delete;
  SignoutScreenshotHandler& operator=(const SignoutScreenshotHandler&) = delete;
  virtual ~SignoutScreenshotHandler();

  // Takes a screenshot of the windows on the active desk and writes it to disk.
  // Invokes `done_callback` when done. Virtual for testing.
  virtual void TakeScreenshot(base::OnceClosure done_callback);

  void set_screenshot_path_for_test(const base::FilePath& path) {
    screenshot_path_for_test_ = path;
  }

  gfx::Size screenshot_size_for_test() { return screenshot_size_; }

 private:
  // Callback invoked when the screenshot is taken. gfx::Image is cheap to pass
  // by value.
  void OnScreenshotTaken(gfx::Image image);

  // Saves the screenshot to disk.
  void SaveScreenshot(scoped_refptr<base::RefCountedMemory> png_data);

  // Callback invoked after the screenshot is saved.
  void OnScreenshotSaved();

  // Deletes an existing screenshot from disk.
  void DeleteScreenshot();

  // Callback invoked after the screenshot is deleted.
  void OnScreenshotDeleted();

  // Returns the path to the screenshot file.
  base::FilePath GetScreenshotPath() const;

  // Invoked when the screenshot is done.
  base::OnceClosure done_callback_;

  // Time when the screenshot process started.
  base::TimeTicks start_time_;

  // Size of the output screenshot.
  gfx::Size screenshot_size_;

  base::FilePath screenshot_path_for_test_;

  base::WeakPtrFactory<SignoutScreenshotHandler> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_SIGNOUT_SCREENSHOT_HANDLER_H_
