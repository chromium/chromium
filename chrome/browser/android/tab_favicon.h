// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_FAVICON_H_
#define CHROME_BROWSER_ANDROID_TAB_FAVICON_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "third_party/skia/include/core/SkBitmap.h"

class TabAndroid;

namespace content {
class WebContents;
}

namespace favicon {
class FaviconDriver;
}

// Native Favicon provider for Tab. Managed by Java layer.
class TabFavicon : public favicon::FaviconDriverObserver {
 public:
  static SkBitmap GetBitmapForTab(TabAndroid* tab_android);

  TabFavicon(JNIEnv* env,
             TabAndroid* tab_android,
             int navigation_transition_favicon_size);
  ~TabFavicon() override;

  void SetWebContents(JNIEnv* env, content::WebContents* web_contents);
  void ResetWebContents(JNIEnv* env);
  void OnDestroyed(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetFavicon(JNIEnv* env);

  // favicon::FaviconDriverObserver
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override;

 private:
  const int navigation_transition_favicon_size_;
  raw_ptr<content::WebContents> active_web_contents_ = nullptr;

  // Rather than a ScopedJavaGlobalRef, we use a raw_ptr to an object we can
  // easily look up the TabFavicon Java object from. This reduces entries in the
  // finite global ref table.
  raw_ptr<TabAndroid> tab_android_;
  raw_ptr<favicon::FaviconDriver> favicon_driver_;
};

#endif  // CHROME_BROWSER_ANDROID_TAB_FAVICON_H_
