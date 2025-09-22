// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ONE_TIME_TOKENS_ANDROID_TOUCH_TO_FILL_ONE_TIME_TOKENS_BRIDGE_IMPL_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ONE_TIME_TOKENS_ANDROID_TOUCH_TO_FILL_ONE_TIME_TOKENS_BRIDGE_IMPL_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/touch_to_fill/autofill/one_time_tokens/android/touch_to_fill_one_time_tokens_bridge.h"

class TouchToFillOneTimeTokensDelegate;

// The implementation of the JNI bridge for the one-time tokens Touch-To-Fill.
class TouchToFillOneTimeTokensBridgeImpl
    : public TouchToFillOneTimeTokensBridge {
 public:
  TouchToFillOneTimeTokensBridgeImpl();
  ~TouchToFillOneTimeTokensBridgeImpl() override;

  bool Show(content::WebContents* web_contents,
            TouchToFillOneTimeTokensDelegate* delegate,
            const std::u16string& token) override;

  void Hide() override;

  // Methods called from Java.
  void OnDismissed(JNIEnv* env, bool token_accepted);
  void OnTokenAccepted(JNIEnv* env,
                       const base::android::JavaParamRef<jstring>& token);
  void OnTokenRejected(JNIEnv* env);

 private:
  raw_ptr<TouchToFillOneTimeTokensDelegate> delegate_;
  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ONE_TIME_TOKENS_ANDROID_TOUCH_TO_FILL_ONE_TIME_TOKENS_BRIDGE_IMPL_H_
