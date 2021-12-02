// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_test_util.h"

#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "storage/common/file_system/file_system_types.h"
#include "url/origin.h"

namespace sharesheet {

apps::mojom::IntentPtr CreateValidTextIntent() {
  return apps_util::CreateShareIntentFromText(kTestText, kTestTitle);
}

apps::mojom::IntentPtr CreateValidUrlIntent() {
  return apps_util::CreateShareIntentFromText(kTestUrl, kTestTitle);
}

apps::mojom::IntentPtr CreateInvalidIntent() {
  auto intent = apps::mojom::Intent::New();
  intent->action = apps_util::kIntentActionSend;
  return intent;
}

apps::mojom::IntentPtr CreateDriveIntent() {
  return apps_util::CreateShareIntentFromDriveFile(GURL(kTestUrl), "image/",
                                                   GURL(kTestUrl), false);
}

storage::FileSystemURL FileInDownloads(Profile* profile, base::FilePath file) {
  url::Origin origin =
      url::Origin::Create(file_manager::util::GetFileManagerURL());
  std::string mount_point_name =
      file_manager::util::GetDownloadsMountPointName(profile);
  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  mount_points->RegisterFileSystem(
      mount_point_name, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(),
      file_manager::util::GetDownloadsFolderForProfile(profile));
  return mount_points->CreateExternalFileSystemURL(blink::StorageKey(origin),
                                                   mount_point_name, file);
}

storage::FileSystemURL FileInNonNativeFileSystemType(Profile* profile,
                                                     base::FilePath file) {
  return storage::FileSystemURL::CreateForTest(
      blink::StorageKey::CreateFromStringForTesting("chrome-extension://xxx"),
      storage::kFileSystemTypeExternal,
      base::FilePath("arc-documents-provider").Append(file), "",
      storage::kFileSystemTypeArcDocumentsProvider, base::FilePath(), "",
      storage::FileSystemMountOption());
}

}  // namespace sharesheet
