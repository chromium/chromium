// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_
#define CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_

#include <memory>
#include <set>
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
      const GURL& primary_icon_url);

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

  // Returns the ideal size for an adaptive launcher icon of a WebAPK
  static int GetIdealAdaptiveLauncherIconSizeInPx();

  // Returns the ideal size for a shortcut icon of a WebAPK.
  static int GetIdealShortcutIconSizeInPx();

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

  // Returns true if there is an installed WebAPK which can handle |url|.
  static bool IsWebApkInstalled(content::BrowserContext* browser_context,
                                const GURL& url);

  // Returns true if there is a WebAPK installed under |origin|, and false
  // otherwise.
  static bool DoesOriginContainAnyInstalledWebApk(const GURL& origin);

  // Returns true if there is a TWA installed under |origin|, and false
  // otherwise.
  static bool DoesOriginContainAnyInstalledTrustedWebActivity(
      const GURL& origin);

  // Returns a set of origins that have an installed WebAPK or TWA.
  static std::set<GURL> GetOriginsWithInstalledWebApksOrTwas();

  // Sets a flag to force an update for the WebAPK corresponding to |id| on next
  // launch.
  static void SetForceWebApkUpdate(const std::string& id);

  // Returns if the Android version supports Adaptive Icon (i.e. API level >=
  // 26)
  static bool DoesAndroidSupportMaskableIcons();

  static void SetIdealShortcutSizeForTesting(int size);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ShortcutHelper);
};

#endif  // CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_
