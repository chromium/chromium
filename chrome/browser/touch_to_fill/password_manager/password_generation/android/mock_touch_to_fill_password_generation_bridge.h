// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_PASSWORD_GENERATION_ANDROID_MOCK_TOUCH_TO_FILL_PASSWORD_GENERATION_BRIDGE_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_PASSWORD_GENERATION_ANDROID_MOCK_TOUCH_TO_FILL_PASSWORD_GENERATION_BRIDGE_H_

#include <jni.h>

#include "chrome/browser/touch_to_fill/password_manager/password_generation/android/touch_to_fill_password_generation_bridge.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockTouchToFillPasswordGenerationBridge
    : public TouchToFillPasswordGenerationBridge {
 public:
  MockTouchToFillPasswordGenerationBridge();
  ~MockTouchToFillPasswordGenerationBridge() override;

  MOCK_METHOD(bool,
              Show,
              (content::WebContents*,
               PrefService*,
               TouchToFillPasswordGenerationDelegate*,
               std::u16string,
               std::string),
              (override));
  MOCK_METHOD(void, Hide, (), (override));
  MOCK_METHOD(void, OnDismissed, (JNIEnv*, bool), (override));
  MOCK_METHOD(void,
              OnGeneratedPasswordAccepted,
              (JNIEnv*, const base::android::JavaParamRef<jstring>&),
              (override));
  MOCK_METHOD(void, OnGeneratedPasswordRejected, (JNIEnv*), (override));
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_PASSWORD_GENERATION_ANDROID_MOCK_TOUCH_TO_FILL_PASSWORD_GENERATION_BRIDGE_H_
