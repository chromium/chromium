// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_TAB_UNDERLINE_MANAGER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_TAB_UNDERLINE_MANAGER_H_

#include <map>
#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/glic/browser_ui/tab_underline_controller.h"
#include "third_party/jni_zero/jni_zero.h"

namespace android {

class TabUnderlineManager {
 public:
  TabUnderlineManager(JNIEnv* env, const jni_zero::JavaRef<jobject>& obj);
  ~TabUnderlineManager();

  TabUnderlineManager(const TabUnderlineManager&) = delete;
  TabUnderlineManager& operator=(const TabUnderlineManager&) = delete;

  // Destroy the native manager.
  void Destroy(JNIEnv* env);

  // Register a tab to be tracked for underlines.
  void RegisterTab(JNIEnv* env, const jni_zero::JavaRef<jobject>& jtab);

  // Unregister a tab from being tracked.
  void UnregisterTab(JNIEnv* env, int32_t tab_id);

  // Sets whether the tab has an active underline.
  void SetUnderlineState(int tab_id, bool is_underlined);

  // Resets the underline animation cycle for the tab.
  void ResetAnimationCycle(int tab_id);

 private:
  class UiDelegateImpl;

  struct TabIndicatorContext {
    TabIndicatorContext();
    TabIndicatorContext(
        std::unique_ptr<glic::TabUnderlineController> controller,
        std::unique_ptr<UiDelegateImpl> delegate);
    ~TabIndicatorContext();

    // Destroying the delegate after the controller ensures the controller can
    // safely reference its delegate during teardown.
    std::unique_ptr<UiDelegateImpl> delegate;
    std::unique_ptr<glic::TabUnderlineController> controller;
  };

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  std::map<int, TabIndicatorContext> tracked_tabs_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_TAB_UNDERLINE_MANAGER_H_
