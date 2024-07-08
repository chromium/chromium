// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEED_ANDROID_FEED_SERVICE_BRIDGE_H_
#define CHROME_BROWSER_FEED_ANDROID_FEED_SERVICE_BRIDGE_H_

#include <string>

#include "base/android/jni_android.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/public/unread_content_observer.h"
#include "url/gurl.h"

namespace feed {

// Native access to |FeedServiceBridge| in Java.
class FeedServiceBridge {
 public:
  static std::string GetLanguageTag();
  static DisplayMetrics GetDisplayMetrics();
  static void ClearAll();
  static bool IsEnabled();
  static void PrefetchImage(const GURL& url);
  static uint64_t GetReliabilityLoggingId();
  static bool IsSignedIn();
};

class JavaUnreadContentObserver : public UnreadContentObserver {
 public:
  JavaUnreadContentObserver(
      base::android::ScopedJavaGlobalRef<jobject> j_observer);
  ~JavaUnreadContentObserver() override;

  void HasUnreadContentChanged(bool has_unread_content) override;
  void Destroy(JNIEnv*);

 private:
  base::android::ScopedJavaGlobalRef<jobject> obj_;
};

}  // namespace feed

#endif  // CHROME_BROWSER_FEED_ANDROID_FEED_SERVICE_BRIDGE_H_
