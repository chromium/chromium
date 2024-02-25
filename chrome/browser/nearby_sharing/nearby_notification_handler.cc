// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_notification_handler.h"

#include <utility>

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/nearby_sharing/nearby_notification_delegate.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "components/cross_device/logging/logging.h"

namespace {

NearbyNotificationDelegate* GetNotificationDelegate(
    Profile* profile,
    const std::string& notification_id) {
  DCHECK(NearbySharingServiceFactory::IsNearbyShareSupportedForBrowserContext(
      profile));

  return NearbySharingServiceFactory::GetForBrowserContext(profile)
      ->GetNotificationDelegate(notification_id);
}

void CloseNearbyNotification(Profile* profile,
                             const std::string& notification_id) {
  NotificationDisplayServiceFactory::GetInstance()
      ->GetForProfile(profile)
      ->Close(NotificationHandler::Type::NEARBY_SHARE, notification_id);
}

}  // namespace

NearbyNotificationHandler::NearbyNotificationHandler() = default;

NearbyNotificationHandler::~NearbyNotificationHandler() = default;

void NearbyNotificationHandler::OnClick(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    const std::optional<int>& action_index,
    const std::optional<std::u16string>& reply,
    base::OnceClosure completed_closure) {
  NearbyNotificationDelegate* delegate =
      GetNotificationDelegate(profile, notification_id);
  if (!delegate) {
    CD_LOG(VERBOSE, Feature::NS)
        << "Ignoring notification click event for unknown id "
        << notification_id;
    CloseNearbyNotification(profile, notification_id);
    std::move(completed_closure).Run();
    return;
  }

  delegate->OnClick(notification_id, action_index);
  std::move(completed_closure).Run();
}

void NearbyNotificationHandler::OnClose(Profile* profile,
                                        const GURL& origin,
                                        const std::string& notification_id,
                                        bool by_user,
                                        base::OnceClosure completed_closure) {
  NearbyNotificationDelegate* delegate =
      GetNotificationDelegate(profile, notification_id);
  if (!delegate) {
    CD_LOG(VERBOSE, Feature::NS)
        << "Ignoring notification close event for unknown id "
        << notification_id;
    std::move(completed_closure).Run();
    return;
  }

  delegate->OnClose(notification_id);
  std::move(completed_closure).Run();
}

void NearbyNotificationHandler::OpenSettings(Profile* profile,
                                             const GURL& origin) {
  DCHECK(NearbySharingServiceFactory::IsNearbyShareSupportedForBrowserContext(
      profile));
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile, chromeos::settings::mojom::kNearbyShareSubpagePath);
}
