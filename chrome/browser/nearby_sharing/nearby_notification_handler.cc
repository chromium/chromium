// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_notification_handler.h"

#include <utility>

#include "base/callback.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/nearby_notification_delegate.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#endif

namespace {

NearbyNotificationDelegate* GetNotificationDelegate(
    Profile* profile,
    const std::string& notification_id) {
  NearbySharingService* nearby_service =
      NearbySharingServiceFactory::GetForBrowserContext(profile);
  if (!nearby_service)
    return nullptr;

  return nearby_service->GetNotificationDelegate(notification_id);
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
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply,
    base::OnceClosure completed_closure) {
  NearbyNotificationDelegate* delegate =
      GetNotificationDelegate(profile, notification_id);
  if (!delegate) {
    NS_LOG(VERBOSE) << "Ignoring notification click event for unknown id "
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
    NS_LOG(VERBOSE) << "Ignoring notification close event for unknown id "
                    << notification_id;
    std::move(completed_closure).Run();
    return;
  }

  delegate->OnClose(notification_id);
  std::move(completed_closure).Run();
}

void NearbyNotificationHandler::OpenSettings(Profile* profile,
                                             const GURL& origin) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile, chromeos::settings::mojom::kNearbyShareSubpagePath);
#else
  // TODO(crbug.com/1102348): Open browser settings once there is a nearby page.
  NOTREACHED();
#endif
}
