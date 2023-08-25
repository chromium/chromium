// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_BRIDGE_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_BRIDGE_H_

#include <jni.h>
#include <string>

#include "content/public/browser/web_contents.h"

class TouchToFillPasswordGenerationDelegate;

class TouchToFillPasswordGenerationBridge {
 public:
  virtual ~TouchToFillPasswordGenerationBridge() = default;

  virtual bool Show(content::WebContents* web_contents,
                    TouchToFillPasswordGenerationDelegate* delegate,
                    std::u16string password,
                    std::string account) = 0;
  virtual void Hide() = 0;
  virtual void OnDismissed(JNIEnv* env) = 0;
  virtual void OnGeneratedPasswordAccepted(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& password) = 0;
  virtual void OnGeneratedPasswordRejected(JNIEnv* env) = 0;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_BRIDGE_H_
