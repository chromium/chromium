// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HUMAN_PRESENCE_SNOOPING_PROTECTION_NOTIFICATION_BLOCKER_INTERNAL_H_
#define ASH_SYSTEM_HUMAN_PRESENCE_SNOOPING_PROTECTION_NOTIFICATION_BLOCKER_INTERNAL_H_

// Logic exposed only for testing on which clients of the notification blocker
// cannot rely.

#include <string>

#include "ash/strings/grit/ash_strings.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {
namespace hps_internal {

// Returns a human-readable title for the given notification source. Improper
// nouns are always returned lower-case and must be subsequently capitalized if
// necessary.
//
// Takes a template argument to allow injection of a duck-typed fake app
// registry cache.
template <typename AppRegistryCacheWrapperType>
std::u16string GetNotifierTitle(const message_center::NotifierId& id,
                                const AccountId& account_id) {
  std::u16string title = l10n_util::GetStringUTF16(
      IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_SYSTEM_TITLE_LOWER);

  // Assign default title based on notifier type.
  switch (id.type) {
    case message_center::NotifierType::APPLICATION:
      title = l10n_util::GetStringUTF16(
          IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_APP_TITLE_LOWER);
      break;

    case message_center::NotifierType::ARC_APPLICATION:
      title = l10n_util::GetStringUTF16(
          IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_ARC_TITLE);
      break;

    case message_center::NotifierType::CROSTINI_APPLICATION:
      title = l10n_util::GetStringUTF16(
          IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_CROSTINI_TITLE);
      break;

    case message_center::NotifierType::WEB_PAGE:
      title = id.title.value_or(l10n_util::GetStringUTF16(
          IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_WEB_TITLE_LOWER));
      break;

    case message_center::NotifierType::SYSTEM_COMPONENT:
      // Handled by initial value.
      break;

    case message_center::NotifierType::PHONE_HUB:
      title = l10n_util::GetStringUTF16(
          IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_PHONE_HUB_TITLE);
      break;
  }

  // If we can access a more-specific app title, assign it here.
  if (id.type == message_center::NotifierType::APPLICATION ||
      id.type == message_center::NotifierType::ARC_APPLICATION ||
      id.type == message_center::NotifierType::CROSTINI_APPLICATION) {
    // Access the registry of human-readable app names.
    auto* app_cache =
        AppRegistryCacheWrapperType::Get().GetAppRegistryCache(account_id);

    if (!app_cache) {
      LOG(ERROR) << "Couldn't find app registry cache for user";
      return title;
    }

    const bool found =
        app_cache->ForOneApp(id.id, [&](const apps::AppUpdate& update) {
          const std::string& app_name = update.Name();
          title = std::u16string(app_name.begin(), app_name.end());
        });

    if (!found)
      LOG(WARNING) << "No matching notifier found for ID " << id.id;
  }

  return title;
}

std::u16string ASH_EXPORT
GetTitlesBlockedMessage(const std::vector<std::u16string>& titles);

}  // namespace hps_internal
}  // namespace ash

#endif  // ASH_SYSTEM_HUMAN_PRESENCE_SNOOPING_PROTECTION_NOTIFICATION_BLOCKER_INTERNAL_H_
