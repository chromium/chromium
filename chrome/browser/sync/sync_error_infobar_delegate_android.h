// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_ERROR_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_SYNC_SYNC_ERROR_INFOBAR_DELEGATE_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class SyncErrorInfoBarDelegateAndroid : public ConfirmInfoBarDelegate {
 public:
  SyncErrorInfoBarDelegateAndroid();
  ~SyncErrorInfoBarDelegateAndroid() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  std::u16string GetMessageText() const override;
  bool Accept() override;
  void InfoBarDismissed() override;

  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_delegate_;

  DISALLOW_COPY_AND_ASSIGN(SyncErrorInfoBarDelegateAndroid);
};

#endif  // CHROME_BROWSER_SYNC_SYNC_ERROR_INFOBAR_DELEGATE_ANDROID_H_
