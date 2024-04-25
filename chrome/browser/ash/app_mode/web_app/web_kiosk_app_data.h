// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_WEB_APP_WEB_KIOSK_APP_DATA_H_
#define CHROME_BROWSER_ASH_APP_MODE_WEB_APP_WEB_KIOSK_APP_DATA_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/kiosk_app_data_base.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/account_id/account_id.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace web_app {
struct WebAppInstallInfo;
}  // namespace web_app

namespace ash {

class KioskAppDataDelegate;

extern const int kWebKioskIconSize;

class WebKioskAppData : public KioskAppDataBase {
 public:
  enum class Status {
    kInit,       // Data initialized with app id.
    kLoading,    // Loading data from cache or web store.
    kLoaded,     // Data load finished.
    kInstalled,  // Icon and launch url are fetched and app can be run
                 // without them.
  };

  WebKioskAppData(KioskAppDataDelegate* delegate,
                  const std::string& app_id,
                  const AccountId& account_id,
                  const GURL url,
                  const std::string& title,
                  const GURL icon_url);
  WebKioskAppData(const WebKioskAppData&) = delete;
  WebKioskAppData& operator=(const WebKioskAppData&) = delete;
  ~WebKioskAppData() override;

  // Loads the locally cached data. Returns true on success.
  bool LoadFromCache();

  // Updates `icon_` from either `KioskAppDataBase::icon_path_` or `icon_url_`.
  void LoadIcon();

  // Get a proper URL to launch according to the app status.
  GURL GetLaunchableUrl() const;

  // Updates `status_`. Based on `notify`, we will notify `delegate_` about data
  // update.
  void SetStatus(Status status, bool notify = true);

  void UpdateFromWebAppInfo(const web_app::WebAppInstallInfo& app_info);

  void UpdateAppInfo(const std::string& title,
                     const GURL& start_url,
                     const web_app::IconBitmaps& icon_bitmaps);

  void SetOnLoadedCallbackForTesting(base::OnceClosure callback);

  Status status() const { return status_; }
  const GURL& install_url() const { return install_url_; }
  const GURL& launch_url() const { return launch_url_; }

 private:
  class IconFetcher;
  void OnDidDownloadIcon(const SkBitmap& icon);
  void OnIconLoadDone(std::optional<gfx::ImageSkia> icon);

  bool LoadLaunchUrlFromDictionary(const base::Value::Dict& dict);

  // Returns the icon url of the icon that was being provided during previous
  // session.
  GURL GetLastIconUrl(const base::Value::Dict& dict) const;

  raw_ptr<KioskAppDataDelegate> delegate_;  // not owned.
  Status status_;
  const GURL install_url_;  // installation url.
  GURL launch_url_;         // app launch url.

  // Url for loading the app icon in case the app has not been installed earlier
  // and a user opened the App menu in the login screen.
  GURL icon_url_;
  // Used to download icon from `icon_url_`.
  std::unique_ptr<IconFetcher> icon_fetcher_;

  base::OnceClosure on_loaded_closure_for_testing_;

  base::WeakPtrFactory<WebKioskAppData> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_WEB_APP_WEB_KIOSK_APP_DATA_H_
