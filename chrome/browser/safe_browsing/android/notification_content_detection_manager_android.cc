// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/notification_content_detection_manager_android.h"

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_constants.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/platform_notification_context.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/blink/public/common/notifications/notification_resources.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace safe_browsing {

void UpdateSuspiciousNotificationIds(HostContentSettingsMap* hcsm,
                                     const GURL& origin,
                                     std::string notification_id) {
  CHECK(hcsm);
  CHECK(origin.is_valid());
  // Get the current value of the setting to append the notification id.
  base::Value cur_value(hcsm->GetWebsiteSetting(
      origin, origin, ContentSettingsType::SUSPICIOUS_NOTIFICATION_IDS));
  base::Value::Dict dict = cur_value.is_dict() ? std::move(cur_value.GetDict())
                                               : base::Value::Dict();
  base::Value::List notification_id_list =
      dict.FindList(safe_browsing::kSuspiciousNotificationIdsKey)
          ? std::move(
                *dict.FindList(safe_browsing::kSuspiciousNotificationIdsKey))
          : base::Value::List();
  notification_id_list.Append(notification_id);
  // Set the updated value in the host content settings map.
  dict.Set(safe_browsing::kSuspiciousNotificationIdsKey,
           base::Value::List(std::move(notification_id_list)));
  hcsm->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(origin),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::SUSPICIOUS_NOTIFICATION_IDS,
      base::Value(std::move(dict)));
}

}  // namespace safe_browsing
