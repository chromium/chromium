// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_FAVICON_H_
#define CHROME_BROWSER_ANDROID_TAB_FAVICON_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/favicon/core/favicon_driver_observer.h"

namespace content {
class WebContents;
}

namespace favicon {
class FaviconDriver;
}

// Native Favicon provider for Tab. Managed by Java layer.
class TabFavicon : public favicon::FaviconDriverObserver {
 public:
  TabFavicon(JNIEnv* env,
             const base::android::JavaParamRef<jobject>& obj,
             int navigation_transition_favicon_size);
  ~TabFavicon() override;

  void SetWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jweb_contents);
  void ResetWebContents(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);
  void OnDestroyed(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jobject> GetFavicon(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // favicon::FaviconDriverObserver
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override;

 private:
  const int navigation_transition_favicon_size_;
  raw_ptr<content::WebContents> active_web_contents_ = nullptr;

  base::android::ScopedJavaGlobalRef<jobject> jobj_;
  raw_ptr<favicon::FaviconDriver> favicon_driver_;
};

#endif  // CHROME_BROWSER_ANDROID_TAB_FAVICON_H_
