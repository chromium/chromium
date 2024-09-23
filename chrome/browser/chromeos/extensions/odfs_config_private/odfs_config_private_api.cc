// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/odfs_config_private/odfs_config_private_api.h"

#include <string>
#include <vector>

#include "chrome/browser/chromeos/enterprise/cloud_storage/policy_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/odfs_config_private.h"
#include "chromeos/constants/chromeos_features.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/ash/cloud_upload/automated_mount_error_notification.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#else
#error Unsupported platform.
#endif

namespace extensions {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
namespace {
constexpr char kUnsupportedAshVersion[] =
    "Cannot show notification because ash version is not supported";
}  // namespace
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

OdfsConfigPrivateGetMountFunction::OdfsConfigPrivateGetMountFunction() =
    default;

OdfsConfigPrivateGetMountFunction::~OdfsConfigPrivateGetMountFunction() =
    default;

ExtensionFunction::ResponseAction OdfsConfigPrivateGetMountFunction::Run() {
  extensions::api::odfs_config_private::MountInfo metadata;
  metadata.mode = chromeos::cloud_storage::GetMicrosoftOneDriveMount(
      Profile::FromBrowserContext(browser_context()));
  return RespondNow(ArgumentList(
      extensions::api::odfs_config_private::GetMount::Results::Create(
          metadata)));
}

OdfsConfigPrivateGetAccountRestrictionsFunction::
    OdfsConfigPrivateGetAccountRestrictionsFunction() = default;

OdfsConfigPrivateGetAccountRestrictionsFunction::
    ~OdfsConfigPrivateGetAccountRestrictionsFunction() = default;

ExtensionFunction::ResponseAction
OdfsConfigPrivateGetAccountRestrictionsFunction::Run() {
  base::Value::List restrictions =
      chromeos::cloud_storage::GetMicrosoftOneDriveAccountRestrictions(
          Profile::FromBrowserContext(browser_context()));
  std::vector<std::string> restrictions_vector;
  for (auto& restriction : restrictions) {
    if (restriction.is_string()) {
      restrictions_vector.emplace_back(std::move(restriction.GetString()));
    }
  }

  extensions::api::odfs_config_private::AccountRestrictionsInfo metadata;
  metadata.restrictions = std::move(restrictions_vector);
  return RespondNow(
      ArgumentList(extensions::api::odfs_config_private::
                       GetAccountRestrictions::Results::Create(metadata)));
}

OdfsConfigPrivateShowAutomatedMountErrorFunction::
    OdfsConfigPrivateShowAutomatedMountErrorFunction() = default;

OdfsConfigPrivateShowAutomatedMountErrorFunction::
    ~OdfsConfigPrivateShowAutomatedMountErrorFunction() = default;

ExtensionFunction::ResponseAction
OdfsConfigPrivateShowAutomatedMountErrorFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::cloud_upload::ShowAutomatedMountErrorNotification(
      *Profile::FromBrowserContext(browser_context()));
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* const service = chromeos::LacrosService::Get();
  if (!service->IsRegistered<crosapi::mojom::OneDriveNotificationService>() ||
      !service->IsAvailable<crosapi::mojom::OneDriveNotificationService>()) {
    return RespondNow(Error(kUnsupportedAshVersion));
  }

  service->GetRemote<crosapi::mojom::OneDriveNotificationService>()
      ->ShowAutomatedMountError();
#else
#error Unsupported platform.
#endif
  return RespondNow(NoArguments());
}

OdfsConfigPrivateIsCloudFileSystemEnabledFunction::
    OdfsConfigPrivateIsCloudFileSystemEnabledFunction() = default;

OdfsConfigPrivateIsCloudFileSystemEnabledFunction::
    ~OdfsConfigPrivateIsCloudFileSystemEnabledFunction() = default;

ExtensionFunction::ResponseAction
OdfsConfigPrivateIsCloudFileSystemEnabledFunction::Run() {
  return RespondNow(ArgumentList(
      api::odfs_config_private::IsCloudFileSystemEnabled::Results::Create(
          chromeos::features::IsFileSystemProviderCloudFileSystemEnabled())));
}

OdfsConfigPrivateIsContentCacheEnabledFunction::
    OdfsConfigPrivateIsContentCacheEnabledFunction() = default;

OdfsConfigPrivateIsContentCacheEnabledFunction::
    ~OdfsConfigPrivateIsContentCacheEnabledFunction() = default;

ExtensionFunction::ResponseAction
OdfsConfigPrivateIsContentCacheEnabledFunction::Run() {
  return RespondNow(ArgumentList(
      api::odfs_config_private::IsContentCacheEnabled::Results::Create(
          chromeos::features::IsFileSystemProviderContentCacheEnabled())));
}

}  // namespace extensions
