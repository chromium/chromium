// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_USAGE_INDICATOR_SERVICE_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_USAGE_INDICATOR_SERVICE_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/webapps/common/web_app_id.h"

class NotificationDisplayService;
class PrefService;

namespace web_app {
class WebAppProvider;
}  // namespace web_app

namespace multi_capture {

class MultiCaptureUsageIndicatorService : public KeyedService {
 public:
  struct AllowListedAppNames {
    AllowListedAppNames(
        std::map<webapps::AppId, std::string> show_capture_notification_apps,
        std::map<webapps::AppId, std::string> skip_capture_notification_apps,
        std::map<webapps::AppId, std::string>
            current_capture_notification_apps);
    ~AllowListedAppNames();

    std::map<webapps::AppId, std::string> future_capture_notification_apps;
    std::map<webapps::AppId, std::string> future_capture_no_notification_apps;
    std::map<webapps::AppId, std::string> current_capture_notification_apps;
  };

  ~MultiCaptureUsageIndicatorService() override;

  static std::unique_ptr<MultiCaptureUsageIndicatorService> Create(
      PrefService* prefs,
      web_app::WebAppProvider* provider,
      NotificationDisplayService* notification_display_service);

  void MultiCaptureStarted(const std::string& label,
                           const webapps::AppId& app_id);
  void MultiCaptureStopped(const std::string& label);

 protected:
  explicit MultiCaptureUsageIndicatorService(
      PrefService* prefs,
      web_app::WebAppProvider* provider,
      NotificationDisplayService* notification_display_service);

 private:
  FRIEND_TEST_ALL_PREFIXES(
      MultiCaptureUsageIndicatorBrowserTest,
      YouMayBeCapturedNotificationShowsIfAppInstalledAndAllowlisted);
  FRIEND_TEST_ALL_PREFIXES(
      MultiCaptureUsageIndicatorBrowserTest,
      YouAreCapturedNotificationShowsIfAppInstalledAndAllowlisted);

  void ShowUsageIndicatorsOnStart();
  AllowListedAppNames GetInstalledAndAllowlistedAppNames() const;
  void ShowFutureMultiCaptureNotification(const AllowListedAppNames& apps);
  void ShowActiveMultiCaptureNotifications(const AllowListedAppNames& apps);

  void RefreshNotifications();

  // As the keyed service is bound to the profile / browser context and the
  // web app provider keyed service is listed as dependency for this service,
  // these raw pointers are safe because the profile and provider objects are
  // guaranteed by the keyed service system to be alive at least until the
  // `Shutdown` function is called.
  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<web_app::WebAppProvider> provider_;
  const raw_ptr<NotificationDisplayService> notification_display_service_;
  base::Value::List multi_screen_capture_allow_list_on_login_;

  // Stores started captures and stores a mapping `app_id` --> `label`.
  std::map<webapps::AppId, std::set<std::string>> started_captures_;
  std::map<std::string, webapps::AppId> label_to_app_id_;

  base::WeakPtrFactory<MultiCaptureUsageIndicatorService> weak_ptr_factory_{
      this};
};

}  // namespace multi_capture

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_USAGE_INDICATOR_SERVICE_H_
