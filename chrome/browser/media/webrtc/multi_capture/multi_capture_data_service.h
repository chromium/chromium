// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_DATA_SERVICE_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_DATA_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/webapps/common/web_app_id.h"

class PrefService;

namespace web_app {
class IwaKeyDistributionInfoProvider;
class WebAppProvider;
}  // namespace web_app

namespace multi_capture {

// This keyed service serves as mechanism to prevent dynamic propagation for the
// data that constrains access to the `getAllScreensMedia` API. This is
// intentional to prevent that additional apps can use the API after the user
// was informed on login.
class MultiCaptureDataService : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void MultiCaptureDataChanged() = 0;
    virtual void MultiCaptureDataServiceDestroyed() = 0;

   protected:
    ~Observer() override = default;
  };

  ~MultiCaptureDataService() override;

  static std::unique_ptr<MultiCaptureDataService> Create(
      web_app::WebAppProvider* provider,
      PrefService* prefs);

  const std::map<webapps::AppId, std::string>& GetCaptureAppsWithNotification()
      const;
  const std::map<webapps::AppId, std::string>&
  GetCaptureAppsWithoutNotification() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  FRIEND_TEST_ALL_PREFIXES(
      MultiCaptureUsageIndicatorBrowserTest,
      YouMayBeCapturedNotificationShowsIfAppInstalledAndAllowlisted);
  FRIEND_TEST_ALL_PREFIXES(
      MultiCaptureUsageIndicatorBrowserTest,
      YouAreCapturedNotificationShowsIfAppInstalledAndAllowlisted);

  MultiCaptureDataService(web_app::WebAppProvider* provider,
                          PrefService* prefs);

  void Init();
  void LoadData();

  const raw_ref<web_app::IwaKeyDistributionInfoProvider> info_provider_;
  const raw_ptr<web_app::WebAppProvider> provider_;
  const raw_ptr<PrefService> prefs_;

  bool is_initialized_ = false;

  base::Value::List multi_screen_capture_allowlist_on_login_;
  std::vector<std::string> app_without_notification_bundle_ids_;

  std::map<webapps::AppId, std::string> capture_apps_with_notification_;
  std::map<webapps::AppId, std::string> capture_apps_without_notification_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<MultiCaptureDataService> weak_ptr_factory_{this};
};

}  // namespace multi_capture

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_DATA_SERVICE_H_
