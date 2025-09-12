// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_USAGE_INDICATOR_SERVICE_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_USAGE_INDICATOR_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/media/webrtc/multi_capture/multi_capture_data_service.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/multi_capture_notification_details_view.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/webapps/common/web_app_id.h"

class PrefService;
class Profile;

namespace message_center {
class Notification;
}

namespace multi_capture {

class MultiCaptureUsageIndicatorService
    : public KeyedService,
      public MultiCaptureDataService::Observer {
 public:
  struct AllowListedAppNames {
    AllowListedAppNames(
        std::map<webapps::AppId, std::string> future_capture_notification_apps,
        std::map<webapps::AppId, std::string>
            future_capture_no_notification_apps,
        std::map<webapps::AppId, std::string>
            current_capture_notification_apps);
    ~AllowListedAppNames();

    std::map<webapps::AppId, std::string> future_capture_notification_apps;
    std::map<webapps::AppId, std::string> future_capture_no_notification_apps;
    std::map<webapps::AppId, std::string> current_capture_notification_apps;
  };

  ~MultiCaptureUsageIndicatorService() override;

  static std::unique_ptr<MultiCaptureUsageIndicatorService> Create(
      Profile* profile,
      PrefService* prefs,
      MultiCaptureDataService* data_service);

  void MultiCaptureStarted(const std::string& label,
                           const webapps::AppId& app_id);
  void MultiCaptureStopped(const std::string& label);

  // MultiCaptureDataService::Observer:
  void MultiCaptureDataChanged() override;
  void MultiCaptureDataServiceDestroyed() override;

 protected:
  explicit MultiCaptureUsageIndicatorService(
      Profile* profile,
      PrefService* prefs,
      MultiCaptureDataService* data_service);

 private:
  FRIEND_TEST_ALL_PREFIXES(
      MultiCaptureUsageIndicatorBrowserTest,
      YouMayBeCapturedNotificationShowsIfAppInstalledAndAllowlisted);
  FRIEND_TEST_ALL_PREFIXES(
      MultiCaptureUsageIndicatorBrowserTest,
      YouAreCapturedNotificationShowsIfAppInstalledAndAllowlisted);

  message_center::Notification CreateFutureCaptureNotification(
      const MultiCaptureUsageIndicatorService::AllowListedAppNames& apps);
  message_center::Notification CreateActiveCaptureNotification(
      const webapps::AppId& app_id,
      const std::string& app_name,
      bool should_reuse_future_notification_id);

  AllowListedAppNames GetInstalledAndAllowlistedAppNames() const;
  void ShowFutureMultiCaptureNotification(const AllowListedAppNames& apps);
  void ShowActiveMultiCaptureNotifications(const AllowListedAppNames& apps);

  void RefreshNotifications();

  std::vector<MultiCaptureNotificationDetailsView::AppInfo>
  GetAllCaptureWithNotificationApps(
      const MultiCaptureUsageIndicatorService::AllowListedAppNames& apps) const;
  std::vector<MultiCaptureNotificationDetailsView::AppInfo>
  GetAllCaptureWithoutNotificationApps(
      const MultiCaptureUsageIndicatorService::AllowListedAppNames& apps) const;

  NotificationDisplayService& notification_display_service() const;

  // As the keyed service is bound to the browser context and the
  // multi capture data service and notification display service keyed
  // servicesare listed as dependency for this service, these raw pointers are
  // safe because the profile and provider objects are guaranteed by the keyed
  // service system to be alive at least until the `Shutdown` function is
  // called.
  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<MultiCaptureDataService> data_service_;
  const raw_ptr<Profile> profile_;
  base::Value::List multi_screen_capture_allow_list_on_login_;

  // Stores started captures and stores a mapping `app_id` --> `label`.
  std::map<webapps::AppId, std::set<std::string>> started_captures_;
  // Stores a mapping from `label` to the `app_id` of the app that is capturing
  // with that `label`.
  std::map<std::string, webapps::AppId> label_to_app_id_;
  std::set<webapps::AppId> notification_shown_for_app_id_;

  base::ScopedObservation<MultiCaptureDataService,
                          MultiCaptureDataService::Observer>
      data_service_observer_{this};

  base::WeakPtrFactory<MultiCaptureUsageIndicatorService> weak_ptr_factory_{
      this};
};

}  // namespace multi_capture

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_USAGE_INDICATOR_SERVICE_H_
