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
// The sheet cannot be closed by swiping.
class AccountStorageNotice : public CoordinatorObserver {
 public:
  // Shows the notice. `accepted_cb` must be non-null and will be invoked once
  // the user clicks "Got it" and the sheet is closed. The object does nothing
  // else after that, so it's safe for the callback to destroy the object.
  AccountStorageNotice(content::WebContents* web_contents,
                       base::OnceClosure accepted_cb);

  AccountStorageNotice(const AccountStorageNotice&) = delete;
  AccountStorageNotice& operator=(const AccountStorageNotice&) = delete;

  // By the time this object is destroyed, normally the user should've clicked
  // "Got it", thus closing the sheet and invoking `accepted_cb`. If that didn't
  // happen, the destructor will hide the sheet promptly without invoking
  // `accepted_cb`. (That's why the callback isn't called `closed_cb` :D)
  ~AccountStorageNotice() override;

 private:
  void OnAccepted(JNIEnv* env) override;

  base::android::ScopedJavaGlobalRef<jobject> java_coordinator_;

  base::OnceClosure accepted_cb_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCOUNT_STORAGE_NOTICE_ACCOUNT_STORAGE_NOTICE_H_
