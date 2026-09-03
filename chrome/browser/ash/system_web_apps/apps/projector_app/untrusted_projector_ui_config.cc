// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/projector_app/untrusted_projector_ui_config.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/channel/channel_info.h"
#include "chromeos/ash/components/signin/identity_manager_provider.h"
#include "components/account_id/account_id.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/version_info/channel.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui_data_source.h"
#include "third_party/blink/public/common/features.h"

ChromeUntrustedProjectorUIDelegate::ChromeUntrustedProjectorUIDelegate(
    const ApplicationLocaleStorage* application_locale_storage)
    : application_locale_storage_(CHECK_DEREF(application_locale_storage)) {}

ChromeUntrustedProjectorUIDelegate::~ChromeUntrustedProjectorUIDelegate() =
    default;

void ChromeUntrustedProjectorUIDelegate::PopulateLoadTimeData(
    content::WebUIDataSource* source) {
  version_info::Channel channel = ash::GetChannel();
  source->AddBoolean("isDevChannel", channel == version_info::Channel::DEV);
  source->AddBoolean("isDebugMode", ash::features::IsProjectorAppDebugMode());
  source->AddBoolean("isCustomThumbnailEnabled",
                     ash::features::IsProjectorCustomThumbnailEnabled());
  // The local playback feature depends on the file handling API.
  source->AddBoolean(
      "isLocalPlaybackEnabled",
      base::FeatureList::IsEnabled(blink::features::kFileHandlingAPI));
  source->AddBoolean("isMutingEnabled",
                     ash::features::IsProjectorMutingEnabled());
  source->AddBoolean("isPwaRedirectEnabled",
                     ash::features::IsProjectorRedirectToPwaEnabled());
  source->AddBoolean("useDvsPlaybackEndpoint",
                     ash::features::IsProjectorUseDVSPlaybackEndpointEnabled());

  source->AddBoolean(
      "isInternalServerSideSpeechRecognitionEnabled",
      ash::features::IsInternalServerSideSpeechRecognitionEnabled());
  source->AddString("appLocale", application_locale_storage_->Get());
}

UntrustedProjectorUIConfig::UntrustedProjectorUIConfig(
    const ApplicationLocaleStorage* application_locale_storage)
    : SystemWebAppUntrustedUIConfig(ash::kChromeUIProjectorAppHost,
                                    ash::SystemWebAppType::PROJECTOR),
      application_locale_storage_(CHECK_DEREF(application_locale_storage)) {}

UntrustedProjectorUIConfig::~UntrustedProjectorUIConfig() = default;

std::unique_ptr<content::WebUIController>
UntrustedProjectorUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                                  const GURL& url) {
  ChromeUntrustedProjectorUIDelegate delegate(
      &application_locale_storage_.get());
  Profile* profile = Profile::FromWebUI(web_ui);
  // Projector is only enabled for profiles with a GAIA account (see
  // IsProjectorAllowedForProfile()), so this profile is guaranteed to have
  // an annotated account.
  const AccountId& account_id =
      CHECK_DEREF(ash::AnnotatedAccountId::Get(profile));
  return std::make_unique<ash::UntrustedProjectorUI>(
      web_ui, &delegate, profile->GetPrefs(),
      ash::IdentityManagerProvider::Get().Find(account_id),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess()
          .get());
}
