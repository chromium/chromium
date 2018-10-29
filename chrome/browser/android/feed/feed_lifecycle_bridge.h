// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEED_FEED_LIFECYCLE_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_FEED_FEED_LIFECYCLE_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/history/core/browser/history_service_observer.h"

namespace history {
class HistoryService;
}

class Profile;

namespace feed {

// Native counterpart of FeedLifecycleBridge.java. Receives lifecycle events
// that originate in native code (namely, history deletion and cached data
// clearing) and forwards them to Java so that the Feed library can be notified.
class FeedLifecycleBridge : public history::HistoryServiceObserver {
 public:
  explicit FeedLifecycleBridge(Profile* profile);
  ~FeedLifecycleBridge() override;

  void Destroy(JNIEnv* env, const base::android::JavaRef<jobject>& j_this);

  // Overridden from history::HistoryServiceObserver.
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

  // Triggers clearing of all data cached by the Feed library. This should only
  // be called if Feed is enabled.
  static void ClearCachedData();

 private:
  // Reference to the Java half of this bridge. Always valid.
  base::android::ScopedJavaGlobalRef<jobject> j_this_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(FeedLifecycleBridge);
};

}  // namespace feed

#endif  // CHROME_BROWSER_ANDROID_FEED_FEED_LIFECYCLE_BRIDGE_H_
