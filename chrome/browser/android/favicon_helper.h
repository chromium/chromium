// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FAVICON_HELPER_H_
#define CHROME_BROWSER_ANDROID_FAVICON_HELPER_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "url/gurl.h"

class Profile;

class FaviconHelper {
 public:
  FaviconHelper();
  void Destroy(JNIEnv* env);

  FaviconHelper(const FaviconHelper&) = delete;
  FaviconHelper& operator=(const FaviconHelper&) = delete;

  jboolean GetComposedFaviconImage(
      JNIEnv* env,
      Profile* profile,
      std::vector<GURL>& gurls,
      jint j_desired_size_in_pixel,
      const base::android::JavaParamRef<jobject>& j_favicon_image_callback);
  jboolean GetLocalFaviconImageForURL(
      JNIEnv* env,
      Profile* profile,
      GURL& page_url,
      jint j_desired_size_in_pixel,
      const base::android::JavaParamRef<jobject>& j_favicon_image_callback);
  jboolean GetForeignFaviconImageForURL(
      JNIEnv* env,
      Profile* profile,
      GURL& page_url,
      jint j_desired_size_in_pixel,
      const base::android::JavaParamRef<jobject>& j_favicon_image_callback);

  void GetLocalFaviconImageForURLInternal(
      favicon::FaviconService* favicon_service,
      GURL url,
      int desired_size_in_pixel,
      favicon_base::FaviconRawBitmapCallback callback_runner);
  void GetComposedFaviconImageInternal(
      favicon::FaviconService* favicon_service,
      std::vector<GURL> urls,
      int desired_size_in_pixel,
      favicon_base::FaviconResultsCallback callback_runner);
  void OnJobFinished(int job_id);

 private:
  FRIEND_TEST_ALL_PREFIXES(FaviconHelperTest, GetLargestSizeIndex);

  virtual ~FaviconHelper();

  class Job;

  static size_t GetLargestSizeIndex(const std::vector<gfx::Size>& sizes);

  // This function is expected to be bound to a WeakPtr<FaviconHelper>, so that
  // it won't be run if the FaviconHelper is deleted and
  // |j_favicon_image_callback| isn't executed in that case.
  void OnFaviconBitmapResultAvailable(
      const base::android::JavaRef<jobject>& j_favicon_image_callback,
      const favicon_base::FaviconRawBitmapResult& result);

  void OnComposedFaviconBitmapResultsAvailable(
      const base::android::JavaRef<jobject>& j_favicon_image_callback,
      const int desired_size_in_pixel,
      const std::vector<favicon_base::FaviconRawBitmapResult>& result);

  std::unique_ptr<base::CancelableTaskTracker> cancelable_task_tracker_;

  std::map<int, std::unique_ptr<Job>> id_to_job_;
  int last_used_job_id_;

  base::WeakPtrFactory<FaviconHelper> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ANDROID_FAVICON_HELPER_H_
