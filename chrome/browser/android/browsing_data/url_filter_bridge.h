// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BROWSING_DATA_URL_FILTER_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_BROWSING_DATA_URL_FILTER_BRIDGE_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"

class GURL;

// A wrapper for GURL->bool predicate used to filter browsing data for deletion
// on Android.
class UrlFilterBridge {
 public:
  explicit UrlFilterBridge(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter);

  UrlFilterBridge(const UrlFilterBridge&) = delete;
  UrlFilterBridge& operator=(const UrlFilterBridge&) = delete;

  ~UrlFilterBridge();

  // Destroys this object.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Whether |jurl| is matched by this filter.
  bool MatchesUrl(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj,
                  std::string& url_spec) const;

  // The Java counterpart of this object.
  const base::android::ScopedJavaGlobalRef<jobject>& j_bridge() const {
    return j_bridge_;
  }

 private:
  // The wrapped native filter.
  base::RepeatingCallback<bool(const GURL&)> url_filter_;

  // The Java counterpart of this C++ object.
  base::android::ScopedJavaGlobalRef<jobject> j_bridge_;
};

#endif // CHROME_BROWSER_ANDROID_BROWSING_DATA_URL_FILTER_BRIDGE_H_
