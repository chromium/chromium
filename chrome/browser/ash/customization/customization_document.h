// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CUSTOMIZATION_CUSTOMIZATION_DOCUMENT_H_
#define CHROME_BROWSER_ASH_CUSTOMIZATION_CUSTOMIZATION_DOCUMENT_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "url/gurl.h"

class PrefRegistrySimple;
class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace extensions {
class ExternalLoader;
}  // namespace extensions

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace ash {

namespace system {
class StatisticsProvider;
}

class CustomizationWallpaperDownloader;
class ServicesCustomizationExternalLoader;

// Friend function to initialize StartupCustomizationDocument for testing.
void InitStartupCustomizationDocumentForTesting(const std::string& manifest);

// Base class for OEM customization document classes.
class CustomizationDocument {
 public:
  CustomizationDocument(const CustomizationDocument&) = delete;
  CustomizationDocument& operator=(const CustomizationDocument&) = delete;

  virtual ~CustomizationDocument();

  // Return true if the document was successfully fetched and parsed.
  bool IsReady() const { return root_.get(); }

 protected:
  explicit CustomizationDocument(const std::string& accepted_version);

  virtual bool LoadManifestFromFile(const base::FilePath& manifest_path);
  virtual bool LoadManifestFromString(const std::string& manifest);

  std::string GetLocaleSpecificString(const std::string& locale,
                                      const std::string& dictionary_name,
                                      const std::string& entry_name) const;

  std::unique_ptr<base::Value::Dict> root_;

  // Value of the "version" attribute that is supported.
  // Otherwise config is not loaded.
  std::string accepted_version_;
};

// OEM startup customization document class.
// Now StartupCustomizationDocument is loaded in c-tor so just after create it
// may be ready or not (if manifest is missing or corrupted) and this state
// won't be changed later (i.e. IsReady() always return the same value).
class StartupCustomizationDocument : public CustomizationDocument {
 public:
  static StartupCustomizationDocument* GetInstance();

  StartupCustomizationDocument(const StartupCustomizationDocument&) = delete;
  StartupCustomizationDocument& operator=(const StartupCustomizationDocument&) =
      delete;

  std::string GetEULAPage(const std::string& locale) const;

  // These methods can be called even if !IsReady(), in this case VPD values
  // will be returned.
  //
  // Raw value of "initial_locale" like initial_locale="en-US,sv,da,fi,no" .
  const std::string& initial_locale() const { return initial_locale_; }

  // Vector of individual locale values.
  const std::vector<std::string>& configured_locales() const;

  // Default locale value (first value in initial_locale list).
  const std::string& initial_locale_default() const;
  const std::string& initial_timezone() const { return initial_timezone_; }
  const std::string& keyboard_layout() const { return keyboard_layout_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(StartupCustomizationDocumentTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(StartupCustomizationDocumentTest, VPD);
  FRIEND_TEST_ALL_PREFIXES(StartupCustomizationDocumentTest, BadManifest);
  FRIEND_TEST_ALL_PREFIXES(ServicesCustomizationDocumentTest, MultiLanguage);
  friend class OobeLocalizationTest;
  friend void InitStartupCustomizationDocumentForTesting(
      const std::string& manifest);
  friend struct base::DefaultSingletonTraits<StartupCustomizationDocument>;

  // C-tor for singleton construction.
  StartupCustomizationDocument();

  // C-tor for test construction.
  StartupCustomizationDocument(system::StatisticsProvider* provider,
                               const std::string& manifest);

  ~StartupCustomizationDocument() override;

  void Init(system::StatisticsProvider* provider);

  std::string initial_locale_;
  std::vector<std::string> configured_locales_;
  std::string initial_timezone_;
  std::string keyboard_layout_;
};

// OEM services customization document class.
// ServicesCustomizationDocument is fetched from network therefore it is not
// ready just after creation. Fetching of the manifest should be initiated
// outside this class by calling StartFetching() or EnsureCustomizationApplied()
// methods.
// User of the file should check IsReady before use it.
class ServicesCustomizationDocument : public CustomizationDocument {
 public:
  static ServicesCustomizationDocument* GetInstance();

  ServicesCustomizationDocument(const ServicesCustomizationDocument&) = delete;
  ServicesCustomizationDocument& operator=(
      const ServicesCustomizationDocument&) = delete;

  // Registers preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Template URL where to fetch OEM services customization manifest from.
  static constexpr char kManifestUrl[] =
      "https://ssl.gstatic.com/chrome/chromeos-customization/%s.json";

  // Return true if the customization was applied. Customization is applied only
  // once per machine.
  static bool WasOOBECustomizationApplied();

  // If customization has not been applied, start fetching and applying.
  void EnsureCustomizationApplied();

  // Returns OnceClosure with the EnsureCustomizationApplied() method.
  base::OnceClosure EnsureCustomizationAppliedClosure();

  // Start fetching customization document.
  void StartFetching();

  // Apply customization and save in machine options that customization was
  // applied successfully. Return true if customization was applied.
  bool ApplyOOBECustomization();

  // Returns true if default wallpaper URL attribute found in manifest.
  // |out_url| is set to attribute value.
  bool GetDefaultWallpaperUrl(GURL* out_url) const;

  // Returns list of default apps.
  std::optional<base::Value::Dict> GetDefaultApps() const;

  // Creates an extensions::ExternalLoader that will provide OEM default apps.
  // Cache of OEM default apps stored in profile preferences.
  ::extensions::ExternalLoader* CreateExternalLoader(Profile* profile);

  // Returns the name of the folder for OEM apps for given |locale|.
  std::string GetOemAppsFolderName(const std::string& locale) const;

  // Initialize instance of ServicesCustomizationDocument for tests that will
  // override singleton until ShutdownForTesting is called.
  static void InitializeForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> factory);

  // Remove instance of ServicesCustomizationDocument for tests.
  static void ShutdownForTesting();

  // These methods are also called by WallpaperManager to get "global default"
  // customized wallpaper path (and to init default wallpaper path from it)
  // before first wallpaper is shown.
  static base::FilePath GetCustomizedWallpaperCacheDir();
  static base::FilePath GetCustomizedWallpaperDownloadedFileName();

  CustomizationWallpaperDownloader* wallpaper_downloader_for_testing() {
    return wallpaper_downloader_.get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ServicesCustomizationDocument>;
  FRIEND_TEST_ALL_PREFIXES(CustomizationWallpaperDownloaderBrowserTest,
                           OEMWallpaperIsPresent);
  FRIEND_TEST_ALL_PREFIXES(CustomizationWallpaperDownloaderBrowserTest,
                           OEMWallpaperRetryFetch);

  typedef std::vector<base::WeakPtr<ServicesCustomizationExternalLoader> >
      ExternalLoaders;

  // Guard for a single application task (wallpaper downloading, for example).
  class ApplyingTask;

  // C-tor for singleton construction.
  ServicesCustomizationDocument();

  // C-tor for test construction.
  explicit ServicesCustomizationDocument(const std::string& manifest);

  ~ServicesCustomizationDocument() override;

  // Save applied state in machine settings.
  static void SetApplied(bool val);

  // Overriden from CustomizationDocument:
  bool LoadManifestFromString(const std::string& manifest) override;

  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  // Initiate file fetching. Wait for online status.
  void StartFileFetch();

  // Initiate file fetching. Don't wait for online status.
  void DoStartFileFetch();

  // Called on UI thread with results of ReadFileInBackground.
  void OnManifestRead(const std::string& manifest);

  // Method called when manifest was successfully loaded.
  void OnManifestLoaded();

  // Returns list of default apps in ExternalProvider format.
  static base::Value::Dict GetDefaultAppsInProviderFormat(
      const base::Value::Dict& root);

  // Update cached manifest for |profile|.
  void UpdateCachedManifest(Profile* profile);

  // Customization document not found for give ID.
  void OnCustomizationNotFound();

  // Set OEM apps folder name for AppListSyncableService for |profile|.
  void SetOemFolderName(Profile* profile, const base::Value::Dict& root);

  // Returns the name of the folder for OEM apps for given |locale|.
  std::string GetOemAppsFolderNameImpl(const std::string& locale,
                                       const base::Value::Dict& root) const;

  // Start download of wallpaper image if needed.
  void StartOEMWallpaperDownload(const GURL& wallpaper_url,
                                 std::unique_ptr<ApplyingTask> applying);

  // Check that current customized wallpaper cache exists. Once wallpaper is
  // downloaded, it's never updated (even if manifest is re-fetched).
  // Start wallpaper download if needed.
  void CheckAndApplyWallpaper();

  // Intermediate function to pass the result of PathExists to ApplyWallpaper.
  void OnCheckedWallpaperCacheExists(std::unique_ptr<bool> exists,
                                     std::unique_ptr<ApplyingTask> applying);

  // Called after downloaded wallpaper has been checked.
  void ApplyWallpaper(bool default_wallpaper_file_exists,
                      std::unique_ptr<ApplyingTask> applying);

  // Set Shell default wallpaper to customized.
  // It's wrapped as a callback and passed as a parameter to
  // CustomizationWallpaperDownloader.
  void OnOEMWallpaperDownloaded(std::unique_ptr<ApplyingTask> applying,
                                bool success,
                                const GURL& wallpaper_url);

  // Register one of Customization applying tasks.
  void ApplyingTaskStarted();

  // Mark task finished and check for "all customization applied".
  void ApplyingTaskFinished(bool success);

  // Services customization manifest URL.
  GURL url_;

  // SimpleURLLoader instance.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // How many times we already tried to fetch customization manifest file.
  int num_retries_;

  // Manifest fetch is already in progress.
  bool load_started_;

  // Delay between checks for network online state. If the optional is empty,
  // the default value for delay is used.
  std::optional<base::TimeDelta> custom_network_delay_ = std::nullopt;

  // Known external loaders.
  ExternalLoaders external_loaders_;

  std::unique_ptr<CustomizationWallpaperDownloader> wallpaper_downloader_;

  // This is barrier until customization is applied.
  // When number of finished tasks match number of started - customization is
  // applied.
  size_t apply_tasks_started_;
  size_t apply_tasks_finished_;

  // This is the number of successfully finished customization tasks.
  // If it matches number of tasks finished - customization is applied
  // successfully.
  size_t apply_tasks_success_;

  // Weak factory for callbacks.
  base::WeakPtrFactory<ServicesCustomizationDocument> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CUSTOMIZATION_CUSTOMIZATION_DOCUMENT_H_
