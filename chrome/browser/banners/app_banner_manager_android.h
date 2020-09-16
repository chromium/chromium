// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_ANDROID_H_
#define CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_ANDROID_H_

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/android/webapps/add_to_homescreen_installer.h"
#include "chrome/browser/banners/app_banner_manager.h"
#include "chrome/browser/installable/installable_ambient_badge_infobar_delegate.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

struct AddToHomescreenParams;

namespace banners {

// Extends the AppBannerManager to support native Android apps. This class owns
// a Java-side AppBannerManager which interfaces with the Java runtime to fetch
// native app data and install them when requested.
//
// A site requests a native app banner by setting "prefer_related_applications"
// to true in its manifest, and providing at least one related application for
// the "play" platform with a Play Store ID.
//
// This class uses that information to request the app's metadata, including an
// icon. If successful, the icon is downloaded and the native app banner shown.
// Otherwise, if no related applications were detected, or their manifest
// entries were invalid, this class falls back to trying to verify if a web app
// banner is suitable.
//
// The code path forks in PerformInstallableCheck(); for a native app, it will
// eventually call to OnAppIconFetched(), while a web app calls through to
// OnDidPerformInstallableCheck(). Each of these methods then calls
// SendBannerPromptRequest(), which combines the forked code paths back
// together.
class AppBannerManagerAndroid
    : public AppBannerManager,
      public InstallableAmbientBadgeInfoBarDelegate::Client,
      public content::WebContentsUserData<AppBannerManagerAndroid> {
 public:
  explicit AppBannerManagerAndroid(content::WebContents* web_contents);
  ~AppBannerManagerAndroid() override;

  using content::WebContentsUserData<AppBannerManagerAndroid>::FromWebContents;

  // Returns a reference to the Java-side AppBannerManager owned by this object.
  const base::android::ScopedJavaLocalRef<jobject> GetJavaBannerManager() const;

  // Returns the name of the installable web app, if the name has been
  // determined (and blank if not).
  base::android::ScopedJavaLocalRef<jstring> GetInstallableWebAppName(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_web_contents);

  // Returns true if the banner pipeline is currently running.
  bool IsRunningForTesting(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& jobj);

  // Called when the Java-side has retrieved information for the app.
  // Returns |false| if an icon fetch couldn't be kicked off.
  bool OnAppDetailsRetrieved(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& japp_data,
      const base::android::JavaParamRef<jstring>& japp_title,
      const base::android::JavaParamRef<jstring>& japp_package,
      const base::android::JavaParamRef<jstring>& jicon_url);

  // AppBannerManager overrides.
  void RequestAppBanner(const GURL& validated_url) override;

  // InstallableAmbientBadgeInfoBarDelegate::Client overrides.
  void AddToHomescreenFromBadge() override;
  void BadgeDismissed() override;

 protected:
  // AppBannerManager overrides.
  std::string GetAppIdentifier() override;
  std::string GetBannerType() override;
  bool IsWebAppConsideredInstalled() override;
  void PerformInstallableChecks() override;
  InstallableParams ParamsToPerformInstallableWebAppCheck() override;
  void PerformInstallableWebAppCheck() override;
  void OnDidPerformInstallableWebAppCheck(
      const InstallableData& result) override;
  void ResetCurrentPageData() override;
  void ShowBannerUi(WebappInstallSource install_source) override;
  void MaybeShowAmbientBadge() override;
  base::WeakPtr<AppBannerManager> GetWeakPtr() override;
  void InvalidateWeakPtrs() override;
  bool IsSupportedAppPlatform(const base::string16& platform) const override;
  bool IsRelatedAppInstalled(
      const blink::Manifest::RelatedApplication& related_app) const override;

 private:
  friend class content::WebContentsUserData<AppBannerManagerAndroid>;

  // Creates the Java-side AppBannerManager.
  void CreateJavaBannerManager(content::WebContents* web_contents);

  // Returns the query value for |name| in |url|, e.g. example.com?name=value.
  std::string ExtractQueryValueForName(const GURL& url,
                                       const std::string& name);

  bool ShouldPerformInstallableNativeAppCheck();
  void PerformInstallableNativeAppCheck();

  // Returns NO_ERROR_DETECTED if |platform|, |url|, and |id| are consistent and
  // can be used to query the Play Store for a native app. Otherwise returns the
  // error which prevents querying from taking place. The query may not
  // necessarily succeed (e.g. |id| doesn't map to anything), but if this method
  // returns NO_ERROR_DETECTED, only a native app banner may be shown, and the
  // web app banner flow will not be run.
  InstallableStatusCode QueryNativeApp(const base::string16& platform,
                                       const GURL& url,
                                       const std::string& id);

  // Called when the download of a native app's icon is complete, as native
  // banners use an icon provided from the Play Store rather than the web
  // manifest.
  void OnNativeAppIconFetched(const SkBitmap& bitmap);

  // Returns the appropriate app name based on whether we have a native/web app.
  base::string16 GetAppName() const override;

  // Hides the ambient badge if it is showing.
  void HideAmbientBadge();

  // Called for recording metrics.
  void RecordEventForAppBanner(AddToHomescreenInstaller::Event event,
                               const AddToHomescreenParams& a2hs_params);

  // The Java-side AppBannerManager.
  base::android::ScopedJavaGlobalRef<jobject> java_banner_manager_;

  // Java-side object containing data about a native app.
  base::android::ScopedJavaGlobalRef<jobject> native_app_data_;

  // App package name for a native app banner.
  std::string native_app_package_;

  // Title to display in the banner for native app.
  base::string16 native_app_title_;

  base::WeakPtrFactory<AppBannerManagerAndroid> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(AppBannerManagerAndroid);
};

}  // namespace banners

#endif  // CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_ANDROID_H_
