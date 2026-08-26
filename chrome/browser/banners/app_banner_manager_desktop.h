// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_DESKTOP_H_
#define CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_DESKTOP_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace extensions {
class ExtensionRegistry;
}

namespace tabs {
class TabInterface;
}

namespace webapps {
enum class InstallResultCode;
class TestAppBannerManagerDesktop;

// Manages web app banners for desktop platforms. Owned by the tab's
// TabFeatures.
class AppBannerManagerDesktop : public AppBannerManager::Delegate,
                                public web_app::WebAppInstallManagerObserver {
 public:
  DECLARE_USER_DATA(AppBannerManagerDesktop);

  AppBannerManagerDesktop(const AppBannerManagerDesktop&) = delete;
  AppBannerManagerDesktop& operator=(const AppBannerManagerDesktop&) = delete;

  ~AppBannerManagerDesktop() override;

  // Creates the manager, honoring the testing factory override.
  // `web_contents` is passed explicitly because during a discard the manager
  // is recreated for the incoming WebContents before `tab` swaps its
  // contents.
  static std::unique_ptr<AppBannerManagerDesktop> Create(
      tabs::TabInterface& tab,
      content::WebContents* web_contents);

  static AppBannerManagerDesktop* From(tabs::TabInterface* tab);

  // Deregisters this manager from the tab's UnownedUserDataHost ahead of its
  // asynchronous destruction during a tab discard, so that the replacement
  // manager can register while observers of the old one detach in their own
  // discard callbacks.
  void DeregisterFromTabForDiscard();

  virtual TestAppBannerManagerDesktop*
  AsTestAppBannerManagerDesktopForTesting();

  AppBannerManager* app_banner_manager() const {
    return app_banner_manager_.get();
  }

 protected:
  AppBannerManagerDesktop(tabs::TabInterface& tab,
                          content::WebContents* web_contents);

  using CreateAppBannerManagerForTesting =
      std::unique_ptr<AppBannerManagerDesktop> (*)(tabs::TabInterface&,
                                                   content::WebContents*);
  static CreateAppBannerManagerForTesting
      override_app_banner_manager_desktop_for_testing_;

  // AppBannerManager::Delegate overrides.
  bool CanRequestAppBanner() const override;
  InstallableParams ParamsToPerformInstallableWebAppCheck() override;
  bool ShouldDoNativeAppCheck(
      const blink::mojom::Manifest& manifest) const override;
  void DoNativeAppInstallableCheck(content::WebContents* web_contents,
                                   const GURL& validated_url,
                                   const blink::mojom::Manifest& manifest,
                                   NativeCheckCallback callback) override;
  void OnWebAppInstallableCheckedNoErrors(
      const ManifestId& manifest_id) override;
  base::expected<void, InstallableStatusCode> CanRunWebAppInstallableChecks(
      const blink::mojom::Manifest& manifest) override;
  bool IsSupportedNonWebAppPlatform(
      const std::u16string& platform) const override;
  bool IsRelatedNonWebAppInstalled(
      const blink::Manifest::RelatedApplication& related_app) const override;
  void MaybeShowAmbientBadge(const InstallBannerConfig& config) override;
  void InvalidateWeakPtrsForThisNavigation() override;
  void ResetCurrentPageData() override;
  void OnMlInstallPrediction(std::string result_label) override;
  AppBannerManager::ShowBannerUiResult ShowBannerUi(
      WebappInstallSource install_source,
      const InstallBannerConfig& config) override;

  // Called when the web app install initiated by a banner has completed.
  void DidFinishCreatingWebApp(
      const webapps::ManifestId& manifest_id,
      base::WeakPtr<AppBannerManagerDesktop> is_navigation_current,
      const webapps::AppId& app_id,
      webapps::InstallResultCode code);

 private:
  web_app::WebAppRegistrar& registrar() const;

  // web_app::WebAppInstallManagerObserver:
  void OnWebAppInstalledWithOsHooks(const webapps::AppId& app_id) override;
  void OnWebAppWillBeUninstalled(const webapps::AppId& app_id) override;
  void OnWebAppUninstalled(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source) override;
  void OnWebAppInstallManagerDestroyed() override;
  void InstallableWebAppStatusUpdate() override;

  void CreateWebApp(WebappInstallSource install_source,
                    web_app::WebAppInstalledCallback install_callback);
  // Catch only kSuccessNewInstall and kUserInstallDeclined user responses if
  // the dialog is triggered by ML.
  void DidCreateWebAppFromMLDialog(const webapps::AppId& app_id,
                                   webapps::InstallResultCode code);

  std::unique_ptr<AppBannerManager> app_banner_manager_;

  raw_ptr<extensions::ExtensionRegistry> extension_registry_;
  webapps::AppId uninstalling_app_id_;

  base::ScopedObservation<web_app::WebAppInstallManager,
                          web_app::WebAppInstallManagerObserver>
      install_manager_observation_{this};

  std::optional<ui::ScopedUnownedUserData<AppBannerManagerDesktop>>
      scoped_unowned_user_data_;

  base::WeakPtrFactory<AppBannerManagerDesktop> weak_factory_{this};
};

}  // namespace webapps

#endif  // CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_DESKTOP_H_
