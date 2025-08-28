// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_DATA_SERVICE_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_DATA_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/webapps/common/web_app_id.h"

class PrefService;

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace web_app {
class IwaKeyDistributionInfoProvider;
class WebAppProvider;
}  // namespace web_app

namespace multi_capture {

// This keyed service serves as mechanism to prevent dynamic propagation for the
// data that constrains access to the `getAllScreensMedia` API. This is
// intentional to prevent that additional apps can use the API after the user
// was informed on login.
class MultiCaptureDataService : public KeyedService,
                                public web_app::WebAppInstallManagerObserver {
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
  gfx::ImageSkia GetAppIcon(const webapps::AppId& app_id) const;

  bool IsMultiCaptureAllowed(const GURL& url) const;
  bool IsMultiCaptureAllowedForAnyApp() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // web_app::WebAppInstallManagerObserver:
  void OnWebAppInstalled(const webapps::AppId& app_id) override;
  void OnWebAppUninstalled(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source) override;
  void OnWebAppInstallManagerDestroyed() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(
      MultiCaptureUsageIndicatorBrowserTest,
      YouMayBeCapturedNotificationShowsIfAppInstalledAndAllowlisted);
  FRIEND_TEST_ALL_PREFIXES(
      MultiCaptureUsageIndicatorBrowserTest,
      YouAreCapturedNotificationShowsIfAppInstalledAndAllowlisted);
  FRIEND_TEST_ALL_PREFIXES(MultiCaptureUsageIndicatorDynamicAppBrowserTest,
                           AllowlistedAppAddedAndDeleted);

  MultiCaptureDataService(web_app::WebAppProvider* provider,
                          PrefService* prefs);

  void Init();
  void LoadData();
  void OnIconReceived(const webapps::AppId& app_id, gfx::ImageSkia icon);
  bool MaybeAddAppToCaptureAppLists(const webapps::AppId& app_id);

  const raw_ref<web_app::IwaKeyDistributionInfoProvider> info_provider_;
  const raw_ptr<web_app::WebAppProvider> provider_;
  const raw_ptr<PrefService> prefs_;

  bool is_initialized_ = false;

  base::Value::List multi_screen_capture_allowlist_on_login_;
  std::set<std::string> app_without_notification_bundle_ids_;

  std::map<webapps::AppId, std::string> capture_apps_with_notification_;
  std::map<webapps::AppId, std::string> capture_apps_without_notification_;
  std::map<webapps::AppId, gfx::ImageSkia> app_icons_;

  base::ScopedObservation<web_app::WebAppInstallManager,
                          web_app::WebAppInstallManagerObserver>
      install_manager_observation_{this};

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<MultiCaptureDataService> weak_ptr_factory_{this};
};

}  // namespace multi_capture

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_DATA_SERVICE_H_
