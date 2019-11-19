// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_DESKTOP_H_
#define CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_DESKTOP_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/banners/app_banner_manager.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace extensions {
class ExtensionRegistry;
}

namespace web_app {
enum class InstallResultCode;
}

namespace banners {

// Manages web app banners for desktop platforms.
class AppBannerManagerDesktop
    : public AppBannerManager,
      public content::WebContentsUserData<AppBannerManagerDesktop>,
      public web_app::AppRegistrarObserver {
 public:
  ~AppBannerManagerDesktop() override;

  using content::WebContentsUserData<AppBannerManagerDesktop>::FromWebContents;

  // Turn off triggering on engagement notifications or navigates, for testing
  // purposes only.
  static void DisableTriggeringForTesting();

 protected:
  explicit AppBannerManagerDesktop(content::WebContents* web_contents);

  // AppBannerManager overrides.
  base::WeakPtr<AppBannerManager> GetWeakPtr() override;
  void InvalidateWeakPtrs() override;
  bool IsSupportedAppPlatform(const base::string16& platform) const override;
  bool IsRelatedAppInstalled(
      const blink::Manifest::RelatedApplication& related_app) const override;

  // Called when the web app install initiated by a banner has completed.
  virtual void DidFinishCreatingWebApp(const web_app::AppId& app_id,
                                       web_app::InstallResultCode code);

 private:
  friend class content::WebContentsUserData<AppBannerManagerDesktop>;
  friend class FakeAppBannerManagerDesktop;

  web_app::AppRegistrar& registrar();

  // AppBannerManager overrides.
  bool IsWebAppConsideredInstalled() override;
  bool ShouldAllowWebAppReplacementInstall() override;
  void ShowBannerUi(WebappInstallSource install_source) override;

  // content::WebContentsObserver override.
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  // SiteEngagementObserver override.
  void OnEngagementEvent(content::WebContents* web_contents,
                         const GURL& url,
                         double score,
                         SiteEngagementService::EngagementType type) override;

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

}  // namespace banners

#endif  // CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_DESKTOP_H_
