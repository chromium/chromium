// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/flash_deprecation_infobar_delegate.h"

#include "base/feature_list.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

// static
void FlashDeprecationInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    HostContentSettingsMap* host_content_settings_map) {
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::make_unique<FlashDeprecationInfoBarDelegate>(
          host_content_settings_map)));
}

// static
bool FlashDeprecationInfoBarDelegate::ShouldDisplayFlashDeprecation(
    HostContentSettingsMap* host_content_settings_map) {
  DCHECK(host_content_settings_map);

  if (!base::FeatureList::IsEnabled(features::kFlashDeprecationWarning))
    return false;

  bool is_managed = false;
  ContentSetting flash_setting =
      PluginUtils::UnsafeGetRawDefaultFlashContentSetting(
          host_content_settings_map, &is_managed);

  // If the user can't do anything about their browser's Flash behavior,
  // there's no point to showing a Flash deprecation warning infobar.
  if (is_managed)
    return false;

  // Display the infobar if the Flash setting is anything other than BLOCK.
  return flash_setting != CONTENT_SETTING_BLOCK;
}

FlashDeprecationInfoBarDelegate::FlashDeprecationInfoBarDelegate(
    HostContentSettingsMap* host_content_settings_map)
    : host_content_settings_map_(host_content_settings_map) {}

infobars::InfoBarDelegate::InfoBarIdentifier
FlashDeprecationInfoBarDelegate::GetIdentifier() const {
  return FLASH_DEPRECATION_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& FlashDeprecationInfoBarDelegate::GetVectorIcon() const {
  return vector_icons::kExtensionIcon;
}

base::string16 FlashDeprecationInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

GURL FlashDeprecationInfoBarDelegate::GetLinkURL() const {
  return GURL(
      "https://www.blog.google/products/chrome/saying-goodbye-flash-chrome/");
}

base::string16 FlashDeprecationInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_PLUGIN_FLASH_DEPRECATION_PROMPT);
}

int FlashDeprecationInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 FlashDeprecationInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(IDS_TURN_OFF);
}

bool FlashDeprecationInfoBarDelegate::Accept() {
  // Can be nullptr in tests.
  if (!host_content_settings_map_)
    return true;

  host_content_settings_map_->SetDefaultContentSetting(
      ContentSettingsType::PLUGINS, CONTENT_SETTING_DEFAULT);
  return true;
}
