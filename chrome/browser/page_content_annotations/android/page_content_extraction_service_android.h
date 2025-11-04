// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_ANDROID_PAGE_CONTENT_EXTRACTION_SERVICE_ANDROID_H_
#define CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_ANDROID_PAGE_CONTENT_EXTRACTION_SERVICE_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"

namespace page_content_annotations {

class PageContentExtractionService;

// JNI bridge for PageContentExtractionService.
class PageContentExtractionServiceAndroid
    : public base::SupportsUserData::Data {
 public:
  explicit PageContentExtractionServiceAndroid(
      PageContentExtractionService* service);
  ~PageContentExtractionServiceAndroid() override;

  // Called from Java.
  void GetAllCachedTabIds(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_callback);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

 private:
  raw_ptr<PageContentExtractionService> service_;

  // A reference to the Java counterpart of this class.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
};

}  // namespace page_content_annotations

#endif  // CHROME_BROWSER_PAGE_CONTENT_ANNOTATIONS_ANDROID_PAGE_CONTENT_EXTRACTION_SERVICE_ANDROID_H_
