// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_CAMERA_APP_CHROME_CAMERA_SAVE_DELEGATE_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_CAMERA_APP_CHROME_CAMERA_SAVE_DELEGATE_H_

#include <cstdint>
#include <variant>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chromeos/ash/experiences/camera/camera_save_handler.h"

namespace content {
class BrowserContext;
}

// Chrome delegate for camera save operations. There is an instance
// of this class per profile.
class ChromeCameraSaveDelegate : public CameraSaveHandler::Delegate {
 public:
  explicit ChromeCameraSaveDelegate(content::BrowserContext* context);

  ChromeCameraSaveDelegate(const ChromeCameraSaveDelegate&) = delete;
  ChromeCameraSaveDelegate& operator=(const ChromeCameraSaveDelegate&) = delete;

  ~ChromeCameraSaveDelegate() override;

 private:
  // CameraSaveHandler::Delegate implementation.
  CameraSaveHandler::FileSaveDestination GetDestination() const override;
  base::FilePath GetMyFilesFolder() const override;

  bool is_onedrive() const {
    return destination_ ==
           policy::local_user_files::FileSaveDestination::kOneDrive;
  }
  bool is_google_drive() const {
    return destination_ ==
           policy::local_user_files::FileSaveDestination::kGoogleDrive;
  }

  const raw_ptr<content::BrowserContext> context_;
  const policy::local_user_files::FileSaveDestination destination_;
  base::WeakPtrFactory<ChromeCameraSaveDelegate> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_CAMERA_APP_CHROME_CAMERA_SAVE_DELEGATE_H_
