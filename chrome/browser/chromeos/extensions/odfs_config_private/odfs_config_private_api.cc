// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/odfs_config_private/odfs_config_private_api.h"

#include <string>
#include <vector>

#include "ash/constants/web_app_id_constants.h"
#include "chrome/browser/chromeos/enterprise/cloud_storage/policy_utils.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/common/extensions/api/odfs_config_private.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/ash/cloud_upload/automated_mount_error_notification.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#else
#error Unsupported platform.
#endif

namespace extensions {

namespace {
constexpr char kMicrosoft365NotInstalled[] =
    "Microsoft 365 PWA is not installed";
constexpr char kReparentingTabFailed[] = "Reparenting tab to M365 failed";
const char kIncognitoError[] =
    "Tabs from guest/incognito mode can't be opened in Office";
#if BUILDFLAG(IS_CHROMEOS_LACROS)
constexpr char kUnsupportedAshVersion[] =
    "Cannot show notification because ash version is not supported";
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}  // namespace

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

OdfsConfigPrivateOpenInOfficeAppFunction::
    OdfsConfigPrivateOpenInOfficeAppFunction() = default;

OdfsConfigPrivateOpenInOfficeAppFunction::
    ~OdfsConfigPrivateOpenInOfficeAppFunction() = default;

ExtensionFunction::ResponseAction
OdfsConfigPrivateOpenInOfficeAppFunction::Run() {
  std::optional<api::odfs_config_private::OpenInOfficeApp::Params> params =
      api::odfs_config_private::OpenInOfficeApp::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());

  // Get the web content for the tab specified by tab_id.
  int tab_id = params->tab_id;
  content::WebContents* web_contents = nullptr;
  if (!ExtensionTabUtil::GetTabById(tab_id, browser_context(),
                                    /*include_incognito=*/true,
                                    &web_contents)) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        ExtensionTabUtil::kTabNotFoundError, base::NumberToString(tab_id))));
  }

  // Tabs in incognito or guest mode should never be reparented.
  Profile* web_contents_profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (web_contents_profile->IsGuestSession() ||
      web_contents_profile->IsOffTheRecord()) {
    return RespondNow(Error(kIncognitoError));
  }

  if (!ash::cloud_upload::IsOfficeWebAppInstalled(profile)) {
    return RespondNow(Error(kMicrosoft365NotInstalled));
  }

  // Open the content of the passed tab inside the M365 PWA.
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile);
  if (!provider->ui_manager().ReparentAppTabToWindow(
          web_contents, ash::kMicrosoft365AppId,
          /*shortcut_created=*/true)) {
    return RespondNow(Error(kReparentingTabFailed));
  }

  return RespondNow(NoArguments());
}

}  // namespace extensions
