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
#include "base/macros.h"
#include "chrome/browser/android/webapk/webapk_info.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/browser/android/webapk/webapk_installer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {
class WebContents;
}

namespace webapps {
struct ShortcutInfo;
}

// ShortcutHelper is the C++ counterpart of org.chromium.chrome.browser's
// ShortcutHelper in Java.
class ShortcutHelper {
 public:
  // Adds a shortcut to the launcher using a SkBitmap. The type of shortcut
  // added depends on the properties in |info|.
  static void AddToLauncherWithSkBitmap(content::WebContents* web_contents,
                                        const webapps::ShortcutInfo& info,
                                        const SkBitmap& icon_bitmap,
                                        bool is_icon_maskable);

  // Shows toast notifying user that a WebAPK install is already in progress
  // when user tries to queue a new install for the same WebAPK.
  static void ShowWebApkInstallInProgressToast();

  // Stores the webapp splash screen in the WebappDataStorage associated with
  // |webapp_id|.
  static void StoreWebappSplashImage(const std::string& webapp_id,
                                     const SkBitmap& splash_image);

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

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ShortcutHelper);
};

#endif  // CHROME_BROWSER_ANDROID_SHORTCUT_HELPER_H_
