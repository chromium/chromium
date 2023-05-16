// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_BRIDGE_IMPL_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_BRIDGE_IMPL_H_

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/touch_to_fill/password_generation/android/touch_to_fill_password_generation_bridge.h"
#include "content/public/browser/web_contents.h"

class TouchToFillPasswordGenerationBridgeImpl
    : public TouchToFillPasswordGenerationBridge {
 public:
  TouchToFillPasswordGenerationBridgeImpl();
  TouchToFillPasswordGenerationBridgeImpl(
      const TouchToFillPasswordGenerationBridgeImpl&) = delete;
  TouchToFillPasswordGenerationBridgeImpl& operator=(
      const TouchToFillPasswordGenerationBridgeImpl&) = delete;
  ~TouchToFillPasswordGenerationBridgeImpl() override;

  bool Show(content::WebContents* web_contents) override;

 private:
  // The corresponding Java TouchToFillCreditCardViewBridge.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_GENERATION_ANDROID_TOUCH_TO_FILL_PASSWORD_GENERATION_BRIDGE_IMPL_H_
