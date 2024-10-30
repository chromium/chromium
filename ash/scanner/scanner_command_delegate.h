// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_COMMAND_DELEGATE_H_
#define ASH_SCANNER_SCANNER_COMMAND_DELEGATE_H_

#include <memory>

#include "ash/ash_export.h"

class GURL;

namespace drive {
class DriveServiceInterface;
}

namespace google_apis {
class RequestSender;
}

namespace ui {
class ClipboardData;
}

namespace ash {

// Delegate for `HandleScannerAction` to access its dependencies for performing
// the command.
class ASH_EXPORT ScannerCommandDelegate {
 public:
  virtual ~ScannerCommandDelegate();

  // Opens the provided URL in the browser.
  virtual void OpenUrl(const GURL& url) = 0;

  // Gets the `DriveServiceInterface` used to upload files.
  virtual drive::DriveServiceInterface* GetDriveService() = 0;

  virtual google_apis::RequestSender* GetGoogleApisRequestSender() = 0;

  // Sets the clipboard to the given `ui::ClipboardData`.
  virtual void SetClipboard(std::unique_ptr<ui::ClipboardData> data) = 0;
};

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_COMMAND_DELEGATE_H_
