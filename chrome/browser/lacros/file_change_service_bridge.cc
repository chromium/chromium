// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/file_change_service_bridge.h"

#include "base/logging.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/crosapi/mojom/file_change_service_bridge.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "storage/browser/file_system/file_system_url.h"

FileChangeServiceBridge::FileChangeServiceBridge(Profile* profile) {
  // NOTE: It is safe to use `base::Unretained(this)` here because the
  // registered callback is guaranteed to only be called within the associated
  // subscription's lifetime.
  file_created_from_show_save_file_picker_subscription_ =
      FileSystemAccessPermissionContextFactory::GetForProfile(profile)
          ->AddFileCreatedFromShowSaveFilePickerCallback(base::BindRepeating(
              &FileChangeServiceBridge::OnFileCreatedFromShowSaveFilePicker,
              base::Unretained(this)));
}

FileChangeServiceBridge::~FileChangeServiceBridge() = default;

void FileChangeServiceBridge::OnFileCreatedFromShowSaveFilePicker(
    const GURL& file_picker_binding_context,
    const storage::FileSystemURL& url) {
  if (url.path().empty()) {
    LOG(WARNING) << "Unexpected `OnFileCreatedFromShowSaveFilePicker()` event.";
    return;
  }
  using Bridge = crosapi::mojom::FileChangeServiceBridge;
  if (auto* service = chromeos::LacrosService::Get();
      service && service->IsAvailable<Bridge>()) {
    service->GetRemote<Bridge>()->OnFileCreatedFromShowSaveFilePicker(
        file_picker_binding_context, url.path());
  }
}
