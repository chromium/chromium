// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCOUNT_STORAGE_NOTICE_ACCOUNT_STORAGE_NOTICE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCOUNT_STORAGE_NOTICE_ACCOUNT_STORAGE_NOTICE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "chrome/browser/password_manager/android/account_storage_notice/coordinator_observer.h"

namespace content {
class WebContents;
}

// A bottom sheet that informs the user they are now saving & filling passwords
// to/from their signed-in account. The sheet contains:
//   - A "Got it" button, which closes the sheet on click.
//   - A link to the settings page, in case they want to disable account
//     passwords via the appropriate toggle. The sheet remains open in the
//     meantime.
// The sheet can also be closed by swiping, pressing back or navigating away.
class AccountStorageNotice : public CoordinatorObserver {
 public:
  // Shows the notice. `closed_cb` must be non-null and will be invoked once the
  // sheet is closed by user interaction. It will not be invoked if the object
  // is prematurely destroyed, see note in the destructor.
  // The object does nothing else after invoking the callback, so it's safe for
  // the callback to destroy the object.
  AccountStorageNotice(content::WebContents* web_contents,
                       base::OnceClosure closed_cb);

  AccountStorageNotice(const AccountStorageNotice&) = delete;
  AccountStorageNotice& operator=(const AccountStorageNotice&) = delete;

  // By the time this object is destroyed, normally the sheet should have been
  // closed and `closed_cb` invoked. If that didn't happen, the destructor will
  // hide the sheet promptly without invoking `closed_cb`.
  ~AccountStorageNotice() override;

 private:
  void OnClosed(JNIEnv* env) override;

  base::android::ScopedJavaGlobalRef<jobject> java_coordinator_;

  base::OnceClosure closed_cb_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCOUNT_STORAGE_NOTICE_ACCOUNT_STORAGE_NOTICE_H_
