// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CAPTURE_MODE_DELEGATE_H_
#define ASH_PUBLIC_CPP_CAPTURE_MODE_DELEGATE_H_

#include "ash/public/cpp/ash_public_export.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash {

// Defines the interface for the delegate of CaptureModeController, that can be
// implemented by an ash client (e.g. Chrome). The CaptureModeController owns
// the instance of this delegate.
class ASH_PUBLIC_EXPORT CaptureModeDelegate {
 public:
  virtual ~CaptureModeDelegate() = default;

  // Returns the path to the active user's downloads directory. This will never
  // be called if the user is not logged in.
  virtual base::FilePath GetActiveUserDownloadsDir() const = 0;

  // Shows the screenshot or screen recording item in the screen capture folder.
  virtual void ShowScreenCaptureItemInFolder(
      const base::FilePath& file_path) = 0;

  // Returns true if the current user is using the 24-hour format (i.e. 14:00
  // vs. 2:00 PM). This is used to build the file name of the captured image or
  // video.
  virtual bool Uses24HourFormat() const = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CAPTURE_MODE_DELEGATE_H_
