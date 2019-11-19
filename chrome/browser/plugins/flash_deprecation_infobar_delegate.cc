// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/flash_deprecation_infobar_delegate.h"

#include <memory>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/core/infobar.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

constexpr base::TimeDelta kMessageCooldown = base::TimeDelta::FromDays(14);

bool IsFlashDeprecationWarningCooldownActive(Profile* profile) {
  base::Time last_dismissal =
      profile->GetPrefs()->GetTime(prefs::kPluginsDeprecationInfobarLastShown);

  // More than |kMessageCooldown| days have passed.
  if (base::Time::Now() - last_dismissal > kMessageCooldown) {
    return false;
  }

  return true;
}

void ActivateFlashDeprecationWarningCooldown(Profile* profile) {
  profile->GetPrefs()->SetTime(prefs::kPluginsDeprecationInfobarLastShown,
                               base::Time::Now());
}

}  // namespace

// static
void FlashDeprecationInfoBarDelegate::Create(InfoBarService* infobar_service,
                                             Profile* profile) {
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::make_unique<FlashDeprecationInfoBarDelegate>(profile)));
}

// static
bool FlashDeprecationInfoBarDelegate::ShouldDisplayFlashDeprecation(
    Profile* profile) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  DCHECK(host_content_settings_map);

  if (!base::FeatureList::IsEnabled(features::kFlashDeprecationWarning))
    return false;

  bool is_managed = false;
  ContentSetting flash_setting =
      PluginUtils::UnsafeGetRawDefaultFlashContentSetting(
          host_content_settings_map, &is_managed);

  // If the user can't do anything about their browser's Flash behavior
  // there's no point to showing a Flash deprecation warning infobar.
  //
  // Also limit showing the infobar to at most once per |kMessageCooldown|.
  // Users should be periodically reminded that they need to take action, but
  // if they couldn't take action and turn off flash it's unlikely they will
  // able to the next time they start a session. The message become more
  // annoying than informative in that case.
  if (is_managed || IsFlashDeprecationWarningCooldownActive(profile)) {
    return false;
  }

  // Display the infobar if the Flash setting is anything other than BLOCK.
  return flash_setting != CONTENT_SETTING_BLOCK;
}

FlashDeprecationInfoBarDelegate::FlashDeprecationInfoBarDelegate(
    Profile* profile)
    : profile_(profile) {}

infobars::InfoBarDelegate::InfoBarIdentifier
FlashDeprecationInfoBarDelegate::GetIdentifier() const {
  return FLASH_DEPRECATION_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& FlashDeprecationInfoBarDelegate::GetVectorIcon() const {
  return kExtensionIcon;
}

base::string16 FlashDeprecationInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_PLUGIN_FLASH_DEPRECATION_PROMPT);
}

int FlashDeprecationInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 FlashDeprecationInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(button, BUTTON_OK);
  return l10n_util::GetStringUTF16(IDS_TURN_OFF);
}

bool FlashDeprecationInfoBarDelegate::Accept() {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  // Can be nullptr in tests.
  if (!host_content_settings_map)
    return true;

  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::PLUGINS, CONTENT_SETTING_DEFAULT);
  return true;
}

base::string16 FlashDeprecationInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

GURL FlashDeprecationInfoBarDelegate::GetLinkURL() const {
  return GURL(
      "https://www.blog.google/products/chrome/saying-goodbye-flash-chrome/");
}

void FlashDeprecationInfoBarDelegate::InfoBarDismissed() {
  ActivateFlashDeprecationWarningCooldown(profile_);
}
