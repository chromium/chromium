// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_supervised_provider.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

namespace {

struct ContentSettingsFromSupervisedSettingsEntry {
  const char* setting_name;
  ContentSettingsType content_type;
  ContentSetting content_setting;
};

const ContentSettingsFromSupervisedSettingsEntry
    kContentSettingsFromSupervisedSettingsMap[] = {
        {
            supervised_users::kGeolocationDisabled,
            ContentSettingsType::GEOLOCATION,
            CONTENT_SETTING_BLOCK,
        },
        {
            supervised_users::kCameraMicDisabled,
            ContentSettingsType::MEDIASTREAM_CAMERA,
            CONTENT_SETTING_BLOCK,
        },
        {
            supervised_users::kCameraMicDisabled,
            ContentSettingsType::MEDIASTREAM_MIC,
            CONTENT_SETTING_BLOCK,
        },
        {
            supervised_users::kCookiesAlwaysAllowed,
            ContentSettingsType::COOKIES,
            CONTENT_SETTING_ALLOW,
        }};

}  // namespace

namespace content_settings {

SupervisedProvider::SupervisedProvider(
    SupervisedUserSettingsService* supervised_user_settings_service) {

  // The SupervisedProvider is owned by the HostContentSettingsMap which
  // DependsOn the SupervisedUserSettingsService (through their factories).
  // This means this will get destroyed before the SUSS and will be
  // unsubscribed from it.
  user_settings_subscription_ =
      supervised_user_settings_service->SubscribeForSettingsChange(
          base::BindRepeating(&content_settings::SupervisedProvider::
                                  OnSupervisedSettingsAvailable,
                              base::Unretained(this)));
}

SupervisedProvider::~SupervisedProvider() = default;

std::unique_ptr<RuleIterator> SupervisedProvider::GetRuleIterator(
    ContentSettingsType content_type,
    bool incognito) const {
  base::AutoLock auto_lock(lock_);
  return value_map_.GetRuleIterator(content_type);
}

void SupervisedProvider::OnSupervisedSettingsAvailable(
    const base::Value::Dict& settings) {
  std::vector<ContentSettingsType> to_notify;
  // Entering locked scope to update content settings.
  {
    base::AutoLock auto_lock(lock_);
    for (const auto& entry : kContentSettingsFromSupervisedSettingsMap) {
      ContentSetting new_setting = CONTENT_SETTING_DEFAULT;
      if (settings.Find(entry.setting_name)) {
        DCHECK(settings.Find(entry.setting_name)->is_bool());
        if (settings.FindBool(entry.setting_name).value_or(false))
          new_setting = entry.content_setting;
      }
      if (new_setting != value_map_.GetContentSetting(entry.content_type)) {
        to_notify.push_back(entry.content_type);
        value_map_.SetContentSetting(entry.content_type, new_setting);
      }
    }
  }
  for (ContentSettingsType type : to_notify) {
    NotifyObservers(ContentSettingsPattern::Wildcard(),
                    ContentSettingsPattern::Wildcard(), type);
  }
}

// Since the SupervisedProvider is a read only content settings provider, all
// methods of the ProviderInterface that set or delete any settings do nothing.
bool SupervisedProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    base::Value&& value,
    const ContentSettingConstraints& constraints) {
  return false;
}

void SupervisedProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
}

void SupervisedProvider::ShutdownOnUIThread() {
  DCHECK(CalledOnValidThread());
  RemoveAllObservers();
  user_settings_subscription_ = {};
}

}  // namespace content_settings
