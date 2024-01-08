// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYNC_SYNC_ERROR_NOTIFIER_H_
#define CHROME_BROWSER_ASH_SYNC_SYNC_ERROR_NOTIFIER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/service/sync_service_observer.h"

class Profile;

namespace ash {

// Shows sync-related errors as notifications in Ash.
class SyncErrorNotifier : public syncer::SyncServiceObserver,
                          public KeyedService {
 public:
  SyncErrorNotifier(syncer::SyncService* sync_service, Profile* profile);

  SyncErrorNotifier(const SyncErrorNotifier&) = delete;
  SyncErrorNotifier& operator=(const SyncErrorNotifier&) = delete;

  ~SyncErrorNotifier() override;

  // KeyedService:
  void Shutdown() override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* service) override;

  const std::string& GetNotificationIdForTesting() const {
    return notification_id_;
  }

 private:
  // The sync service to query for error details.
  raw_ptr<syncer::SyncService> sync_service_ = nullptr;

  // The Profile this service belongs to.
  const raw_ptr<Profile, DanglingUntriaged> profile_;

  // Notification was added to NotificationUIManager. This flag is used to
  // prevent displaying passphrase notification to user if they already saw (and
  // potentially dismissed) previous one.
  bool notification_displayed_ = false;

  // Used to keep track of the message center notification.
  std::string notification_id_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYNC_SYNC_ERROR_NOTIFIER_H_
