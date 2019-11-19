// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FAVICON_HELPER_H_
#define CHROME_BROWSER_ANDROID_FAVICON_HELPER_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_types.h"
#include "url/gurl.h"

namespace content {
class  WebContents;
}

class Profile;

class FaviconHelper {
 public:
  FaviconHelper();
  void Destroy(JNIEnv* env);
  jboolean GetLocalFaviconImageForURL(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_profile,
      const base::android::JavaParamRef<jstring>& j_page_url,
      jint j_desired_size_in_pixel,
      const base::android::JavaParamRef<jobject>& j_favicon_image_callback);
  jboolean GetForeignFaviconImageForURL(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jprofile,
      const base::android::JavaParamRef<jstring>& j_page_url,
      jint j_desired_size_in_pixel,
      const base::android::JavaParamRef<jobject>& j_favicon_image_callback);

  void EnsureIconIsAvailable(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_profile,
      const base::android::JavaParamRef<jobject>& j_web_contents,
      const base::android::JavaParamRef<jstring>& j_page_url,
      const base::android::JavaParamRef<jstring>& j_icon_url,
      jboolean j_is_large_icon,
      const base::android::JavaParamRef<jobject>& j_availability_callback);
  void TouchOnDemandFavicon(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_profile,
      const base::android::JavaParamRef<jstring>& j_icon_url);

 private:
  FRIEND_TEST_ALL_PREFIXES(FaviconHelperTest, GetLargestSizeIndex);

  static void OnFaviconImageResultAvailable(
      const base::android::ScopedJavaGlobalRef<jobject>&
          j_availability_callback,
      Profile* profile,
      content::WebContents* web_contents,
      const GURL& page_url,
      const GURL& icon_url,
      favicon_base::IconType icon_type,
      const favicon_base::FaviconImageResult& result);

  static void OnFaviconDownloaded(
      const base::android::ScopedJavaGlobalRef<jobject>&
          j_availability_callback,
      Profile* profile,
      const GURL& page_url,
      favicon_base::IconType icon_type,
      int download_request_id,
      int http_status_code,
      const GURL& image_url,
      const std::vector<SkBitmap>& bitmaps,
      const std::vector<gfx::Size>& original_sizes);

  static size_t GetLargestSizeIndex(const std::vector<gfx::Size>& sizes);

  std::unique_ptr<base::CancelableTaskTracker> cancelable_task_tracker_;

  virtual ~FaviconHelper();

  DISALLOW_COPY_AND_ASSIGN(FaviconHelper);
};

#endif  // CHROME_BROWSER_ANDROID_FAVICON_HELPER_H_
