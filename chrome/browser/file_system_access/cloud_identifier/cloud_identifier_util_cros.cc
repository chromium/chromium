// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/cloud_identifier/cloud_identifier_util_cros.h"

#include "base/check_is_test.h"
#include "chromeos/crosapi/mojom/file_system_access_cloud_identifier.mojom.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_cloud_identifier.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#else  // BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/file_system_access_cloud_identifier_provider_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

crosapi::mojom::FileSystemAccessCloudIdentifierProvider*
    g_cloud_identifier_provider_for_testing = nullptr;

crosapi::mojom::FileSystemAccessCloudIdentifierProvider*
GetFileSystemAccessCloudIdentifierProvider() {
  if (g_cloud_identifier_provider_for_testing) {
    CHECK_IS_TEST();
    return g_cloud_identifier_provider_for_testing;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service ||
      !lacros_service->IsAvailable<
          crosapi::mojom::FileSystemAccessCloudIdentifierProvider>()) {
    return nullptr;
  }
  return chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::FileSystemAccessCloudIdentifierProvider>()
      .get();
#else
  return crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->file_system_access_cloud_identifier_provider_ash();
#endif
}

crosapi::mojom::HandleType TranslateHandleType(
    content::FileSystemAccessPermissionContext::HandleType handle_type) {
  switch (handle_type) {
    case content::FileSystemAccessPermissionContext::HandleType::kFile:
      return crosapi::mojom::HandleType::kFile;
    case content::FileSystemAccessPermissionContext::HandleType::kDirectory:
      return crosapi::mojom::HandleType::kDirectory;
  }
}

blink::mojom::FileSystemAccessErrorPtr FileSystemAccessErrorOk() {
  return blink::mojom::FileSystemAccessError::New(
      blink::mojom::FileSystemAccessStatus::kOk, base::File::FILE_OK, "");
}

void OnCrosApiResult(
    content::ContentBrowserClient::GetCloudIdentifiersCallback callback,
    crosapi::mojom::FileSystemAccessCloudIdentifierPtr result) {
  if (result.is_null()) {
    std::move(callback).Run(
        blink::mojom::FileSystemAccessError::New(
            blink::mojom::FileSystemAccessStatus::kOperationFailed,
            base::File::Error::FILE_ERROR_FAILED,
            "Unable to retrieve identifier"),
        {});
    return;
  }

  std::vector<blink::mojom::FileSystemAccessCloudIdentifierPtr> handles;
  blink::mojom::FileSystemAccessCloudIdentifierPtr handle =
      blink::mojom::FileSystemAccessCloudIdentifier::New(result->provider_name,
                                                         result->id);
  handles.push_back(std::move(handle));
  std::move(callback).Run(FileSystemAccessErrorOk(), std::move(handles));
}
}  // namespace

namespace cloud_identifier {
void SetCloudIdentifierProviderForTesting(
    crosapi::mojom::FileSystemAccessCloudIdentifierProvider* provider) {
  CHECK_IS_TEST();
  CHECK(!g_cloud_identifier_provider_for_testing);
  g_cloud_identifier_provider_for_testing = provider;
}

void GetCloudIdentifierFromAsh(
    const storage::FileSystemURL& url,
    content::FileSystemAccessPermissionContext::HandleType handle_type,
    content::ContentBrowserClient::GetCloudIdentifiersCallback callback) {
  // Only `kFileSystemTypeDriveFs` and `kFileSystemTypeProvided` can be cloud
  // handled on ChromeOS.
  if (url.type() != storage::kFileSystemTypeDriveFs &&
      url.type() != storage::kFileSystemTypeProvided) {
    std::move(callback).Run(FileSystemAccessErrorOk(), {});
    return;
  }

  crosapi::mojom::FileSystemAccessCloudIdentifierProvider* provider =
      GetFileSystemAccessCloudIdentifierProvider();
  if (!provider) {
    std::move(callback).Run(
        blink::mojom::FileSystemAccessError::New(
            blink::mojom::FileSystemAccessStatus::kOperationFailed,
            base::File::Error::FILE_ERROR_FAILED, "Provider not available"),
        {});
    return;
  }

  provider->GetCloudIdentifier(
      url.virtual_path(), TranslateHandleType(handle_type),
      base::BindOnce(&OnCrosApiResult, std::move(callback)));
}

}  // namespace cloud_identifier
