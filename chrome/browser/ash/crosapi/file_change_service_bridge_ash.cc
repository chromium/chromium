// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/file_change_service_bridge_ash.h"

#include "base/logging.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/fileapi/file_change_service.h"
#include "chrome/browser/ash/fileapi/file_change_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"

namespace crosapi {
namespace {

// Helpers ---------------------------------------------------------------------

// Returns a `storage::FileSystemURL` for the given `profile` and `file_path`.
// Note that the return value must be checked for validity in order to ascertain
// success/failure.
storage::FileSystemURL CreateFileSystemURL(Profile* profile,
                                           const base::FilePath& file_path) {
  if (GURL url; file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile, file_path, file_manager::util::GetFileManagerURL(), &url)) {
    return file_manager::util::GetFileManagerFileSystemContext(profile)
        ->CrackURLInFirstPartyContext(url);
  }
  return storage::FileSystemURL();
}

}  // namespace

// FileChangeServiceBridgeAsh --------------------------------------------------

FileChangeServiceBridgeAsh::FileChangeServiceBridgeAsh(Profile* profile)
    : profile_(profile) {
  profile_observation_.Observe(profile_);
}

FileChangeServiceBridgeAsh::~FileChangeServiceBridgeAsh() = default;

void FileChangeServiceBridgeAsh::BindReceiver(
    mojo::PendingReceiver<mojom::FileChangeServiceBridge> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void FileChangeServiceBridgeAsh::OnProfileWillBeDestroyed(Profile* profile) {
  CHECK_EQ(profile_, profile);
  profile_ = nullptr;
  profile_observation_.Reset();
}

void FileChangeServiceBridgeAsh::OnFileCreatedFromShowSaveFilePicker(
    const GURL& file_picker_binding_context,
    const base::FilePath& file_path) {
  CHECK(profile_);

  if (storage::FileSystemURL file_system_url =
          CreateFileSystemURL(profile_, file_path);
      file_system_url.is_valid()) {
    ash::FileChangeServiceFactory::GetInstance()
        ->GetService(profile_)
        ->NotifyFileCreatedFromShowSaveFilePicker(file_picker_binding_context,
                                                  file_system_url);
  } else {
    LOG(WARNING) << "Unexpected `OnFileCreatedFromShowSaveFilePicker()` event.";
  }
}

}  // namespace crosapi
