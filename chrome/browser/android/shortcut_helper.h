// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_
#define CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/android/shortcut_info.h"
#include "chrome/browser/android/webapk/webapk_info.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/browser/android/webapk/webapk_installer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {
class WebContents;
}

// ShortcutHelper is the C++ counterpart of org.chromium.chrome.browser's
// ShortcutHelper in Java.
class ShortcutHelper {
 public:
  // Creates a ShortcutInfo struct suitable for adding a shortcut to the home
  // screen.
  static std::unique_ptr<ShortcutInfo> CreateShortcutInfo(
      const GURL& manifest_url,
      const blink::Manifest& manifest,
      const GURL& primary_icon_url,
      const GURL& badge_icon_url);

  // Adds a shortcut to the launcher using a SkBitmap. The type of shortcut
  // added depends on the properties in |info|.
  static void AddToLauncherWithSkBitmap(content::WebContents* web_contents,
                                        const ShortcutInfo& info,
                                        const SkBitmap& icon_bitmap,
                                        bool is_icon_maskable);

  // Shows toast notifying user that a WebAPK install is already in progress
  // when user tries to queue a new install for the same WebAPK.
  static void ShowWebApkInstallInProgressToast();

  // Returns the ideal size for an icon representing a web app or a WebAPK.
  static int GetIdealHomescreenIconSizeInPx();

  // Returns the minimum size for an icon representing a web app or a WebAPK.
  static int GetMinimumHomescreenIconSizeInPx();

  // Returns the ideal size for an image displayed on a web app's splash
  // screen.
  static int GetIdealSplashImageSizeInPx();

  // Returns the minimum size for an image displayed on a web app's splash
  // screen.
  static int GetMinimumSplashImageSizeInPx();

  // Returns the ideal size for a badge icon of a WebAPK.
  static int GetIdealBadgeIconSizeInPx();

  // Returns the ideal size for an adaptive launcher icon of a WebAPK
  static int GetIdealAdaptiveLauncherIconSizeInPx();

  // Fetches the splash screen image and stores it inside the WebappDataStorage
  // of the webapp. The WebappDataStorage object *must* have been previously
  // created by AddToLauncherWithSkBitmap(); this method should be passed as a
  // closure to that method.
  static void FetchSplashScreenImage(content::WebContents* web_contents,
                                     const GURL& image_url,
                                     const int ideal_splash_image_size_in_px,
                                     const int minimum_splash_image_size_in_px,
                                     const std::string& webapp_id);

  // Stores the webapp splash screen in the WebappDataStorage associated with
  // |webapp_id|.
  static void StoreWebappSplashImage(const std::string& webapp_id,
                                     const SkBitmap& splash_image);

  // Returns the given icon, modified to match the launcher requirements.
  // This method may generate an entirely new icon; if this is the case,
  // |is_generated| will be set to |true|.
  // Must be called on a background worker thread.
  static SkBitmap FinalizeLauncherIconInBackground(const SkBitmap& icon,
                                                   bool is_icon_maskable,
                                                   const GURL& url,
                                                   bool* is_generated);

  // Returns the package name of one of the WebAPKs which can handle |url|.
  // Returns an empty string if there are no matches.
  static std::string QueryFirstWebApkPackage(const GURL& url);

  // Returns true if WebAPKs are enabled and there is an installed WebAPK which
  // can handle |start_url|, or there is one is being installed.
  static bool IsWebApkInstalled(content::BrowserContext* browser_context,
                                const GURL& start_url,
                                const GURL& manifest_url);

  // Sets a flag to force an update for the WebAPK corresponding to |id| on next
  // launch.
  static void SetForceWebApkUpdate(const std::string& id);

  // Returns if the Android version supports Adaptive Icon (i.e. API level >=
  // 26)
  static bool DoesAndroidSupportMaskableIcons();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ShortcutHelper);
};

#endif  // CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_
