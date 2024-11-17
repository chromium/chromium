// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SUPERVISED_USER_AW_SUPERVISED_USER_SAFEMODE_ACTION_H_
#define ANDROID_WEBVIEW_BROWSER_SUPERVISED_USER_AW_SUPERVISED_USER_SAFEMODE_ACTION_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "url/gurl.h"

namespace android_webview {

// Native side of java-class of same name. Can be called on any thread.
//
// Lifetime: Singleton
class AwSupervisedUserSafeModeAction {
 public:
  static AwSupervisedUserSafeModeAction* GetInstance();
  AwSupervisedUserSafeModeAction(const AwSupervisedUserSafeModeAction&) =
      delete;
  AwSupervisedUserSafeModeAction& operator=(
      const AwSupervisedUserSafeModeAction&) = delete;

  void SetSupervisionEnabled(bool value);
  bool IsSupervisionEnabled();

 private:
  AwSupervisedUserSafeModeAction() = default;
  ~AwSupervisedUserSafeModeAction() = default;

  bool is_supervision_enabled_ = true;
  base::Lock lock_;
  friend class base::NoDestructor<AwSupervisedUserSafeModeAction>;
};
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_SUPERVISED_USER_AW_SUPERVISED_USER_SAFEMODE_ACTION_H_
