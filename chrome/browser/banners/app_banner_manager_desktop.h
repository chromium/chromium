// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_DESKTOP_H_
#define CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_DESKTOP_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "content/public/browser/web_contents_user_data.h"

namespace extensions {
class ExtensionRegistry;
}

namespace web_app {
enum class InstallResultCode;
}

namespace webapps {
class TestAppBannerManagerDesktop;

// Manages web app banners for desktop platforms.
class AppBannerManagerDesktop
    : public AppBannerManager,
      public content::WebContentsUserData<AppBannerManagerDesktop>,
      public web_app::AppRegistrarObserver {
 public:
  ~AppBannerManagerDesktop() override;

  static void CreateForWebContents(content::WebContents* web_contents);
  using content::WebContentsUserData<AppBannerManagerDesktop>::FromWebContents;

  // Turn off triggering on engagement notifications or navigates, for testing
  // purposes only.
  static void DisableTriggeringForTesting();
  virtual TestAppBannerManagerDesktop*
  AsTestAppBannerManagerDesktopForTesting();

 protected:
  explicit AppBannerManagerDesktop(content::WebContents* web_contents);

  using CreateAppBannerManagerForTesting =
      std::unique_ptr<AppBannerManagerDesktop> (*)(content::WebContents*);
  static CreateAppBannerManagerForTesting
      override_app_banner_manager_desktop_for_testing_;

  // AppBannerManager overrides.
  base::WeakPtr<AppBannerManager> GetWeakPtr() override;
  void InvalidateWeakPtrs() override;
  bool IsSupportedNonWebAppPlatform(
      const std::u16string& platform) const override;
  bool IsRelatedNonWebAppInstalled(
      const blink::Manifest::RelatedApplication& related_app) const override;
  bool IsWebAppConsideredInstalled() const override;

  // content::WebContentsObserver override.
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  // Called when the web app install initiated by a banner has completed.
  virtual void DidFinishCreatingWebApp(const web_app::AppId& app_id,
                                       web_app::InstallResultCode code);

 private:
  friend class content::WebContentsUserData<AppBannerManagerDesktop>;
  friend class FakeAppBannerManagerDesktop;

  web_app::AppRegistrar& registrar();

  // AppBannerManager overrides.
  bool ShouldAllowWebAppReplacementInstall() override;
  void ShowBannerUi(WebappInstallSource install_source) override;

  // SiteEngagementObserver override.
  void OnEngagementEvent(content::WebContents* web_contents,
                         const GURL& url,
                         double score,
                         site_engagement::EngagementType type) override;

  // web_app::AppRegistrarObserver:
  void OnWebAppInstalled(const web_app::AppId& app_id) override;
  void OnAppRegistrarDestroyed() override;

  void CreateWebApp(WebappInstallSource install_source);

  extensions::ExtensionRegistry* extension_registry_;

  ScopedObserver<web_app::AppRegistrar, web_app::AppRegistrarObserver>
      registrar_observer_{this};

  base::WeakPtrFactory<AppBannerManagerDesktop> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(AppBannerManagerDesktop);
};

}  // namespace webapps

#endif  // CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_DESKTOP_H_
