// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_ANDROID_ANDROID_PUSH_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_ANDROID_ANDROID_PUSH_NOTIFICATION_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/optimization_guide/core/push_notification_manager.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/push_notification.pb.h"

class PrefService;

namespace optimization_guide {
namespace android {

// This class manages incoming push notifications from Android and any cached
// notifications that were received while native was not alive. Note that the
// only handling that occurs with push notifications is that any previously
// fetched hint (that matches the pushed notification) should be removed from
// in-memory and persistent stores within optimization guide.
//
// Cached notification handling:
// In the Android code, cached notifications are sharded by optimization type.
// Once all the notifications for a given optimization type have been handled,
// the optimization type's cache can be cleared, but this must be explicitly
// requested by native code.
// Memory Management/Overflow: Each optimization type's cache of notifications
// has a maximum size which can overflow. In this
// case, all the cached notifications are lost and the Android layer only
// reports that there was an overflow. When this happens, the entire hint store
// is purged (this should not happen very often).
//
// Push notifications:
// When a notification arrives while native is alive, it is attempted to be
// handled. In the event that backing stores or otherwise are not available when
// the notification arrives, it can be pushed back to the Android layer to be
// cached.
class AndroidPushNotificationManager : public PushNotificationManager {
 public:
  explicit AndroidPushNotificationManager(PrefService* pref_service);
  ~AndroidPushNotificationManager() override;

  // PushNotificationManager implementation:
  void SetDelegate(PushNotificationManager::Delegate* delegate) override;
  void OnDelegateReady() override;
  void OnNewPushNotification(
      const proto::HintNotificationPayload& notification) override;
  void AddObserver(PushNotificationManager::Observer* observer) override;
  void RemoveObserver(PushNotificationManager::Observer* observer) override;

 private:
  // Called when the store needs to be purged because there was a cache
  // overflow.
  void OnNeedToPurgeStore();
  void OnPurgeCompleted();

  // Invalidates hints data based on hints keys in |notification| proto.
  void InvalidateHints(const proto::HintNotificationPayload& notification);

  // Dispatches HintNotificationPayload.payload to observers.
  void DispatchPayload(const proto::HintNotificationPayload& notification);

  // Called when a non-cached notification can't be processed right now so it
  // should be cached in Android.
  void OnNewPushNotificationNotHandled(
      const proto::HintNotificationPayload& notification);

  // Owns |this|. Expected to be set before |OnDelegateReady| is called.
  raw_ptr<PushNotificationManager::Delegate> delegate_ = nullptr;

  // Not owned, but expected to outlive |this|.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // Observers to handle the custom payload.
  base::ObserverList<PushNotificationManager::Observer> observers_;

  base::WeakPtrFactory<AndroidPushNotificationManager> weak_ptr_factory_{this};
};

}  // namespace android
}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_ANDROID_ANDROID_PUSH_NOTIFICATION_MANAGER_H_
