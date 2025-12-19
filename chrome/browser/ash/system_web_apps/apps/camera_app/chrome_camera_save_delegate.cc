// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/camera_app/chrome_camera_save_delegate.h"

#include "base/files/file_path.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/experiences/camera/camera_save_handler.h"

ChromeCameraSaveDelegate::ChromeCameraSaveDelegate(
    content::BrowserContext* context)
    : context_(context),
      destination_(policy::local_user_files::GetCameraDestination(
          Profile::FromBrowserContext(context_))) {}

ChromeCameraSaveDelegate::~ChromeCameraSaveDelegate() = default;

CameraSaveHandler::FileSaveDestination
ChromeCameraSaveDelegate::GetDestination() const {
  switch (destination_) {
    case policy::local_user_files::FileSaveDestination::kOneDrive:
      return CameraSaveHandler::FileSaveDestination::kOneDrive;
    case policy::local_user_files::FileSaveDestination::kGoogleDrive:
      return CameraSaveHandler::FileSaveDestination::kGoogleDrive;
    case policy::local_user_files::FileSaveDestination::kNotSpecified:
    case policy::local_user_files::FileSaveDestination::kDownloads:
      return CameraSaveHandler::FileSaveDestination::kLocal;
  }
}

base::FilePath ChromeCameraSaveDelegate::GetMyFilesFolder() const {
  return file_manager::util::GetMyFilesFolderForProfile(
      Profile::FromBrowserContext(context_));
}
