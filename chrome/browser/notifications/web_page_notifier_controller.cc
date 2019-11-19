// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/web_page_notifier_controller.h"

#include "ash/public/cpp/notifier_metadata.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/notifications/notifier_state_tracker_factory.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/favicon/core/favicon_service.h"

WebPageNotifierController::WebPageNotifierController(Observer* observer)
    : observer_(observer) {}

WebPageNotifierController::~WebPageNotifierController() {}

std::vector<ash::NotifierMetadata> WebPageNotifierController::GetNotifierList(
    Profile* profile) {
  std::vector<ash::NotifierMetadata> notifiers;

  ContentSettingsForOneType settings;
  HostContentSettingsMapFactory::GetForProfile(profile)->GetSettingsForOneType(
      ContentSettingsType::NOTIFICATIONS,
      content_settings::ResourceIdentifier(), &settings);

  favicon::FaviconService* const favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  favicon_tracker_.reset(new base::CancelableTaskTracker());
  patterns_.clear();
  for (ContentSettingsForOneType::const_iterator iter = settings.begin();
       iter != settings.end(); ++iter) {
    if (iter->primary_pattern == ContentSettingsPattern::Wildcard() &&
        iter->secondary_pattern == ContentSettingsPattern::Wildcard() &&
        iter->source != "preference") {
      continue;
    }

    std::string url_pattern = iter->primary_pattern.ToString();
    base::string16 name = base::UTF8ToUTF16(url_pattern);
    GURL url(url_pattern);
    message_center::NotifierId notifier_id(url);
    NotifierStateTracker* const notifier_state_tracker =
        NotifierStateTrackerFactory::GetForProfile(profile);
    content_settings::SettingInfo info;
    HostContentSettingsMapFactory::GetForProfile(profile)->GetWebsiteSetting(
        url, GURL(), ContentSettingsType::NOTIFICATIONS, std::string(), &info);
    notifiers.emplace_back(
        notifier_id, name,
        notifier_state_tracker->IsNotifierEnabled(notifier_id),
        info.source == content_settings::SETTING_SOURCE_POLICY,
        gfx::ImageSkia());
    patterns_[url_pattern] = iter->primary_pattern;
    // Note that favicon service obtains the favicon from history. This means
    // that it will fail to obtain the image if there are no history data for
    // that URL.
    favicon_service->GetFaviconImageForPageURL(
        url,
        base::Bind(&WebPageNotifierController::OnFaviconLoaded,
                   base::Unretained(this), url),
        favicon_tracker_.get());
  }

  return notifiers;
}

void WebPageNotifierController::SetNotifierEnabled(
    Profile* profile,
    const message_center::NotifierId& notifier_id,
    bool enabled) {
  // WEB_PAGE notifier cannot handle in DesktopNotificationService
  // since it has the exact URL pattern.
  // TODO(mukai): fix this.
  ContentSetting default_setting =
      HostContentSettingsMapFactory::GetForProfile(profile)
          ->GetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS, NULL);

  DCHECK(default_setting == CONTENT_SETTING_ALLOW ||
         default_setting == CONTENT_SETTING_BLOCK ||
         default_setting == CONTENT_SETTING_ASK);

  // The content setting for notifications needs to clear when it changes to
  // the default value or get explicitly set when it differs from the
  // default.
  bool differs_from_default_value =
      (default_setting != CONTENT_SETTING_ALLOW && enabled) ||
      (default_setting == CONTENT_SETTING_ALLOW && !enabled);

  if (differs_from_default_value) {
    if (notifier_id.url.is_valid()) {
      NotificationPermissionContext::UpdatePermission(
          profile, notifier_id.url,
          enabled ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
    } else {
      LOG(ERROR) << "Invalid url pattern: "
                 << notifier_id.url.possibly_invalid_spec();
    }
  } else {
    ContentSettingsPattern pattern;

    const auto& iter = patterns_.find(notifier_id.url.possibly_invalid_spec());
    if (iter != patterns_.end()) {
      pattern = iter->second;
    } else if (notifier_id.url.is_valid()) {
      pattern = ContentSettingsPattern::FromURLNoWildcard(notifier_id.url);
    } else {
      LOG(ERROR) << "Invalid url pattern: "
                 << notifier_id.url.possibly_invalid_spec();
    }

    if (pattern.IsValid()) {
      // Note that we don't use
      // NotificationPermissionContext::UpdatePermission()
      // here because pattern might be from user manual input and not match
      // the default one used by ClearSetting().
      HostContentSettingsMapFactory::GetForProfile(profile)
          ->SetContentSettingCustomScope(
              pattern, ContentSettingsPattern::Wildcard(),
              ContentSettingsType::NOTIFICATIONS,
              content_settings::ResourceIdentifier(), CONTENT_SETTING_DEFAULT);
    }
  }

  observer_->OnNotifierEnabledChanged(notifier_id, enabled);
}

void WebPageNotifierController::OnFaviconLoaded(
    const GURL& url,
    const favicon_base::FaviconImageResult& favicon_result) {
  observer_->OnIconImageUpdated(message_center::NotifierId(url),
                                favicon_result.image.AsImageSkia());
}
