// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_FEATURE_NOTIFICATION_GUIDE_SERVICE_H_
#define CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_FEATURE_NOTIFICATION_GUIDE_SERVICE_H_

#include <memory>
#include <set>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/supports_user_data.h"
#include "chrome/browser/feature_guide/notifications/feature_type.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client.h"
#include "components/keyed_service/core/keyed_service.h"

namespace notifications {
class NotificationSchedulerClient;
struct NotificationData;
}  // namespace notifications

namespace feature_guide {
namespace features {

// Main feature flag for the feature notification guide feature.
extern const base::Feature kFeatureNotificationGuide;

}  // namespace features

// The central class responsible for managing feature notification guide in
// chrome.
class FeatureNotificationGuideService : public KeyedService,
                                        public base::SupportsUserData {
 public:
  // A delegate to help with chrome layer dependencies, such as providing
  // notification texts, and handling notification interactions.
  class Delegate {
   public:
    // Returns the notification title text associated with the |feature|.
    virtual std::u16string GetNotificationTitle(FeatureType feature) = 0;

    // Called to get the notification body text associated with the |feature|.
    virtual std::u16string GetNotificationMessage(FeatureType feature) = 0;

    // Called when the notification associated with the given |feature| is
    // clicked.
    virtual void OnNotificationClick(FeatureType feature) = 0;

    // Getter/Setter method for the service.
    FeatureNotificationGuideService* GetService();
    void SetService(FeatureNotificationGuideService* service);

    virtual ~Delegate();

   private:
    FeatureNotificationGuideService* service_{nullptr};
  };

  using NotificationDataCallback = base::OnceCallback<void(
      std::unique_ptr<notifications::NotificationData>)>;

  FeatureNotificationGuideService();
  ~FeatureNotificationGuideService() override;

  FeatureNotificationGuideService(const FeatureNotificationGuideService&) =
      delete;
  FeatureNotificationGuideService& operator=(
      const FeatureNotificationGuideService&) = delete;

  // Called during initialization to notify about the already scheduled set of
  // feature notifications.
  virtual void OnSchedulerInitialized(const std::set<std::string>& guids) = 0;

  // Called before the notification is shown.
  virtual void BeforeShowNotification(
      std::unique_ptr<notifications::NotificationData> notification_data,
      NotificationDataCallback callback) = 0;

  // Called when the notification is clicked.
  virtual void OnClick(FeatureType feature) = 0;
};

using ServiceGetter =
    base::RepeatingCallback<FeatureNotificationGuideService*()>;

// Factory method to create the service.
std::unique_ptr<notifications::NotificationSchedulerClient>
CreateFeatureNotificationGuideNotificationClient(ServiceGetter service_getter);

}  // namespace feature_guide

#endif  // CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_FEATURE_NOTIFICATION_GUIDE_SERVICE_H_
