// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_RECENTLY_CLOSED_TABS_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_RECENTLY_CLOSED_TABS_BRIDGE_H_

#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/sessions/core/tab_restore_service_observer.h"

class Profile;

// Provides the list of recently closed tabs to Java.
class RecentlyClosedTabsBridge : public sessions::TabRestoreServiceObserver {
 public:
  RecentlyClosedTabsBridge(base::android::ScopedJavaGlobalRef<jobject> jbridge,
                           Profile* profile);

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  jboolean GetRecentlyClosedTabs(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jtabs,
      jint max_tab_count);
  jboolean OpenRecentlyClosedTab(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jtab,
      jint tab_id,
      jint j_disposition);
  jboolean OpenMostRecentlyClosedTab(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void ClearRecentlyClosedTabs(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);

  // Observer callback for TabRestoreServiceObserver. Notifies the Java bridge
  // that the recently closed tabs list has changed.
  void TabRestoreServiceChanged(sessions::TabRestoreService* service) override;

  // Observer callback when our associated TabRestoreService is destroyed.
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override;

 private:
  ~RecentlyClosedTabsBridge() override;

  // Construct and initialize tab_restore_service_ if it's NULL.
  // tab_restore_service_ may still be NULL, however, in incognito mode.
  void EnsureTabRestoreService();

  // The Java RecentlyClosedBridge.
  base::android::ScopedJavaGlobalRef<jobject> bridge_;

  // The profile whose recently closed tabs are being monitored.
  Profile* profile_;

  // TabRestoreService that we are observing.
  sessions::TabRestoreService* tab_restore_service_;

  DISALLOW_COPY_AND_ASSIGN(RecentlyClosedTabsBridge);
};

#endif  // CHROME_BROWSER_ANDROID_RECENTLY_CLOSED_TABS_BRIDGE_H_
