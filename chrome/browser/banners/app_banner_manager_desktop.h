// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_DESKTOP_H_
#define CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_DESKTOP_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/banners/app_banner_manager.h"
#include "content/public/browser/web_contents_user_data.h"

namespace extensions {
class BookmarkAppHelper;
}

namespace banners {

// Manages web app banners for desktop platforms.
class AppBannerManagerDesktop
    : public AppBannerManager,
      public content::WebContentsUserData<AppBannerManagerDesktop> {
 public:
  ~AppBannerManagerDesktop() override;

  using content::WebContentsUserData<AppBannerManagerDesktop>::FromWebContents;

  static bool IsEnabled();

  // Turn off triggering on engagement notifications or navigates, for testing
  // purposes only.
  static void DisableTriggeringForTesting();

 protected:
  explicit AppBannerManagerDesktop(content::WebContents* web_contents);

  // AppBannerManager overrides.
  void DidFinishCreatingBookmarkApp(
      const extensions::Extension* extension,
      const WebApplicationInfo& web_app_info) override;

 private:
  friend class content::WebContentsUserData<AppBannerManagerDesktop>;

  // AppBannerManager overrides.
  bool IsWebAppConsideredInstalled(content::WebContents* web_contents,
                                   const GURL& validated_url,
                                   const GURL& start_url,
                                   const GURL& manifest_url) override;
  void ShowBannerUi(WebappInstallSource install_source) override;

  // content::WebContentsObserver override.
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  // SiteEngagementObserver override.
  void OnEngagementEvent(content::WebContents* web_contents,
                         const GURL& url,
                         double score,
                         SiteEngagementService::EngagementType type) override;

  std::unique_ptr<extensions::BookmarkAppHelper> bookmark_app_helper_;

  DISALLOW_COPY_AND_ASSIGN(AppBannerManagerDesktop);
};

}  // namespace banners

#endif  // CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_DESKTOP_H_
