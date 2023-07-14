// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_DESKTOP_H_
#define CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_DESKTOP_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "content/public/browser/web_contents_user_data.h"

namespace extensions {
class ExtensionRegistry;
}

namespace webapps {
enum class InstallResultCode;
class TestAppBannerManagerDesktop;

// Manages web app banners for desktop platforms.
class AppBannerManagerDesktop
    : public AppBannerManager,
      public content::WebContentsUserData<AppBannerManagerDesktop>,
      public web_app::WebAppInstallManagerObserver {
 public:
  AppBannerManagerDesktop(const AppBannerManagerDesktop&) = delete;
  AppBannerManagerDesktop& operator=(const AppBannerManagerDesktop&) = delete;

  ~AppBannerManagerDesktop() override;

  static void CreateForWebContents(content::WebContents* web_contents);
  using content::WebContentsUserData<AppBannerManagerDesktop>::FromWebContents;

  virtual TestAppBannerManagerDesktop*
  AsTestAppBannerManagerDesktopForTesting();

 protected:
  explicit AppBannerManagerDesktop(content::WebContents* web_contents);

  using CreateAppBannerManagerForTesting =
      std::unique_ptr<AppBannerManagerDesktop> (*)(content::WebContents*);
  static CreateAppBannerManagerForTesting
      override_app_banner_manager_desktop_for_testing_;

  // AppBannerManager overrides.
  base::WeakPtr<AppBannerManager> GetWeakPtrForThisNavigation() override;
  void InvalidateWeakPtrsForThisNavigation() override;
  bool IsSupportedNonWebAppPlatform(
      const std::u16string& platform) const override;
  bool IsRelatedNonWebAppInstalled(
      const blink::Manifest::RelatedApplication& related_app) const override;
  bool IsWebAppConsideredInstalled() const override;
  bool IsAppFullyInstalledForSiteUrl(const GURL& site_url) const override;
  bool IsAppPartiallyInstalledForSiteUrl(const GURL& site_url) const override;
  void SaveInstallationDismissedForMl(const GURL& manifest_id) override;
  void SaveInstallationIgnoredForMl(const GURL& manifest_id) override;
  void SaveInstallationAcceptedForMl(const GURL& manifest_id) override;
  bool IsMlPromotionBlockedByHistoryGuardrail(const GURL& manifest_id) override;
  void OnMlInstallPrediction(base::PassKey<MLInstallabilityPromoter>,
                             std::string result_label) override;
  segmentation_platform::SegmentationPlatformService*
  GetSegmentationPlatformService() override;

  // Called when the web app install initiated by a banner has completed.
  virtual void DidFinishCreatingWebApp(const web_app::AppId& app_id,
                                       webapps::InstallResultCode code);

 private:
  friend class content::WebContentsUserData<AppBannerManagerDesktop>;
  friend class FakeAppBannerManagerDesktop;

  web_app::WebAppRegistrar& registrar();

  // AppBannerManager overrides.
  bool ShouldAllowWebAppReplacementInstall() override;
  void ShowBannerUi(WebappInstallSource install_source) override;

  // web_app::WebAppInstallManagerObserver:
  void OnWebAppInstalled(const web_app::AppId& app_id) override;
  void OnWebAppWillBeUninstalled(const web_app::AppId& app_id) override;
  void OnWebAppUninstalled(
      const web_app::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source) override;
  void OnWebAppInstallManagerDestroyed() override;

  void CreateWebApp(WebappInstallSource install_source,
                    web_app::WebAppInstalledCallback install_callback);
  // Catch only kSuccessNewInstall and kUserInstallDeclined user responses if
  // the dialog is triggered by ML.
  void DidCreateWebAppFromMLDialog(const web_app::AppId& app_id,
                                   webapps::InstallResultCode code);

  raw_ptr<segmentation_platform::SegmentationPlatformService>
      segmentation_platform_service_;
  raw_ptr<extensions::ExtensionRegistry> extension_registry_;
  web_app::AppId uninstalling_app_id_;

  base::ScopedObservation<web_app::WebAppInstallManager,
                          web_app::WebAppInstallManagerObserver>
      install_manager_observation_{this};

  base::WeakPtrFactory<AppBannerManagerDesktop> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace webapps

#endif  // CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_DESKTOP_H_
