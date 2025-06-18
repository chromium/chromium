// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_USAGE_INDICATOR_SERVICE_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_USAGE_INDICATOR_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"

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
        std::vector<std::string> show_capture_notification_apps,
        std::vector<std::string> skip_capture_notification_apps);
    ~AllowListedAppNames();

    std::vector<std::string> show_capture_notification_apps;
    std::vector<std::string> skip_capture_notification_apps;
  };

  ~MultiCaptureUsageIndicatorService() override;

  static std::unique_ptr<MultiCaptureUsageIndicatorService> Create(
      PrefService* prefs,
      web_app::WebAppProvider* provider,
      NotificationDisplayService* notification_display_service);

 protected:
  explicit MultiCaptureUsageIndicatorService(
      PrefService* prefs,
      web_app::WebAppProvider* provider,
      NotificationDisplayService* notification_display_service);

 private:
  FRIEND_TEST_ALL_PREFIXES(
      MultiCaptureUsageIndicatorBrowserTest,
      YouMayBeCapturedNotificationShowsIfAppInstalledAndAllowlisted);

  void ShowUsageIndicatorsOnStart();
  AllowListedAppNames GetInstalledAndAllowlistedAppNames() const;
  void ShowFutureMultiCaptureNotification();

  // As the keyed service is bound to the profile / browser context and the
  // web app provider keyed service is listed as dependency for this service,
  // these raw pointers are safe because the profile and provider objects are
  // guaranteed by the keyed service system to be alive at least until the
  // `Shutdown` function is called.
  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<web_app::WebAppProvider> provider_;
  const raw_ptr<NotificationDisplayService> notification_display_service_;
  base::Value::List multi_screen_capture_allow_list_on_login_;

  base::WeakPtrFactory<MultiCaptureUsageIndicatorService> weak_ptr_factory_{
      this};
};

}  // namespace multi_capture

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_USAGE_INDICATOR_SERVICE_H_
