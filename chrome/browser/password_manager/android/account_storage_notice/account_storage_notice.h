// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCOUNT_STORAGE_NOTICE_ACCOUNT_STORAGE_NOTICE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCOUNT_STORAGE_NOTICE_ACCOUNT_STORAGE_NOTICE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "chrome/browser/password_manager/android/account_storage_notice/coordinator_observer.h"

namespace syncer {
class SyncService;
}

namespace ui {
class WindowAndroid;
}

class PrefService;

// A bottom sheet that informs the user they are now saving & filling passwords
// to/from their signed-in account. The sheet contains:
//   - A "Got it" button, which closes the sheet on click.
//   - A link to the settings page, in case they want to disable account
//     passwords via the appropriate toggle. The sheet is auto-closed after the
//     user closes settings.
// The sheet can also be closed by swiping, pressing back or navigating away.
class AccountStorageNotice : public CoordinatorObserver {
 public:
  // This notice should only be shown to users who are signed-in, not syncing,
  // have the passwords data type enabled and never saw it before.
  // If such conditions are not satisfied, this method:
  //   - Shows nothing.
  //   - Invokes `done_cb` immediately.
  //   - Returns null.
  // Otherwise, it:
  //   - Shows the notice.
  //   - Will invoke `done_cb` once the notice is closed by user interaction.
  //   - Returns an object that must be stored by the embedder. `done_cb` can
  //     safely delete the object because:
  //       a) The implementation does nothing else after the invocation.
  //       b) If the object is prematurely destroyed, the notice will close
  //          promptly without invoking the callback. So there will be no double
  //          destruction.
  static std::unique_ptr<AccountStorageNotice> MaybeShow(
      syncer::SyncService* sync_service,
      PrefService* pref_service,
      ui::WindowAndroid* window_android,
      base::OnceClosure done_cb);

  AccountStorageNotice(const AccountStorageNotice&) = delete;
  AccountStorageNotice& operator=(const AccountStorageNotice&) = delete;

  // By the time this object is destroyed, normally the notice should have been
  // closed and `done_cb` invoked. If that didn't happen, the destructor will
  // hide the notice promptly without invoking `done_cb`.
  ~AccountStorageNotice() override;

 private:
  AccountStorageNotice(
      base::android::ScopedJavaLocalRef<jobject> java_coordinator,
      base::OnceClosure done_cb);

  void OnClosed(JNIEnv* env) override;

  const base::android::ScopedJavaGlobalRef<jobject> java_coordinator_;

  base::OnceClosure done_cb_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCOUNT_STORAGE_NOTICE_ACCOUNT_STORAGE_NOTICE_H_
