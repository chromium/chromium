// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_DATA_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_DATA_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_data_base.h"
#include "chrome/browser/extensions/webstore_data_fetcher_delegate.h"
#include "components/account_id/account_id.h"
#include "url/gurl.h"

class Profile;

namespace extensions {
class Extension;
class WebstoreDataFetcher;
}

namespace gfx {
class Image;
}

namespace network {
namespace mojom {
class URLLoaderFactory;
}
}

namespace chromeos {

class KioskAppDataDelegate;

// Fetches an app's web store data and manages the cached info such as name
// and icon.
class KioskAppData : public KioskAppDataBase,
                     public extensions::WebstoreDataFetcherDelegate {
 public:
  enum Status {
    STATUS_INIT,     // Data initialized with app id.
    STATUS_LOADING,  // Loading data from cache or web store.
    STATUS_LOADED,   // Data loaded.
    STATUS_ERROR,    // Failed to load data.
  };

  KioskAppData(KioskAppDataDelegate* delegate,
               const std::string& app_id,
               const AccountId& account_id,
               const GURL& update_url,
               const base::FilePath& cached_crx);
  ~KioskAppData() override;

  // Loads app data from cache. If there is no cached data, fetches it
  // from web store.
  void Load();

  // Loads app data from the app installed in the given profile.
  void LoadFromInstalledApp(Profile* profile, const extensions::Extension* app);

  // Sets full path of the cache crx. The crx would be used to extract meta
  // data for private apps.
  void SetCachedCrx(const base::FilePath& crx_file);

  // Returns true if web store data fetching is in progress.
  bool IsLoading() const;

  // Returns true if the update url points to Webstore.
  bool IsFromWebStore() const;

  const GURL& update_url() const { return update_url_; }
  const std::string& required_platform_version() const {
    return required_platform_version_;
  }
  Status status() const { return status_; }

  void SetStatusForTest(Status status);

  static std::unique_ptr<KioskAppData> CreateForTest(
      KioskAppDataDelegate* delegate,
      const std::string& app_id,
      const AccountId& account_id,
      const GURL& update_url,
      const std::string& required_platform_version);

  // Callbacks for KioskAppIconLoader.
  void OnIconLoadSuccess(const gfx::ImageSkia& icon) override;
  void OnIconLoadFailure() override;

  // Tests do not always fake app data download.
  // This allows to ignore download errors.
  static void SetIgnoreKioskAppDataLoadFailuresForTesting(bool value);

 private:
  class CrxLoader;
  class WebstoreDataParser;

  void SetStatus(Status status);

  // Returns URLLoaderFactory to use for fetching web store data.
  network::mojom::URLLoaderFactory* GetURLLoaderFactory();

  // Loads the locally cached data. Return false if there is none.
  bool LoadFromCache();

  // Sets the cached data.
  void SetCache(const std::string& name,
                const SkBitmap& icon,
                const std::string& required_platform_version);

  // Callback for extensions::ImageLoader.
  void OnExtensionIconLoaded(const gfx::Image& icon);

  // Callbacks for WebstoreDataParser
  void OnWebstoreParseSuccess(const SkBitmap& icon,
                              const std::string& required_platform_version);
  void OnWebstoreParseFailure();

  // Starts to fetch data from web store.
  void StartFetch();

  // extensions::WebstoreDataFetcherDelegate overrides:
  void OnWebstoreRequestFailure() override;
  void OnWebstoreResponseParseSuccess(
      std::unique_ptr<base::DictionaryValue> webstore_data) override;
  void OnWebstoreResponseParseFailure(const std::string& error) override;

  // Helper function for testing for the existence of |key| in
  // |response|. Passes |key|'s content via |value| and returns
  // true when |key| is present.
  bool CheckResponseKeyValue(const base::DictionaryValue* response,
                             const char* key,
                             std::string* value);

  // Extracts meta data from crx file when loading from Webstore and local
  // cache fails.
  void LoadFromCrx();

  void OnCrxLoadFinished(const CrxLoader* crx_loader);

  KioskAppDataDelegate* delegate_;  // not owned.
  Status status_;

  GURL update_url_;
  std::string required_platform_version_;

  std::unique_ptr<extensions::WebstoreDataFetcher> webstore_fetcher_;

  base::FilePath crx_file_;

  base::WeakPtrFactory<KioskAppData> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(KioskAppData);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_DATA_H_
