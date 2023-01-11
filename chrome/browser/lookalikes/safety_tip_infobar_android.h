// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_SAFETY_TIP_INFOBAR_ANDROID_H_
#define CHROME_BROWSER_LOOKALIKES_SAFETY_TIP_INFOBAR_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "components/infobars/android/confirm_infobar.h"

class SafetyTipInfoBarDelegate;

// SafetyTipInfoBar is a thin veneer over ConfirmInfoBar that adds a discrete
// description (instead of just having a title).
class SafetyTipInfoBar : public infobars::ConfirmInfoBar {
 public:
  static std::unique_ptr<infobars::InfoBar> CreateInfoBar(
      std::unique_ptr<SafetyTipInfoBarDelegate> delegate);

  SafetyTipInfoBar(const SafetyTipInfoBar&) = delete;
  SafetyTipInfoBar& operator=(const SafetyTipInfoBar&) = delete;

  ~SafetyTipInfoBar() override;

 private:
  explicit SafetyTipInfoBar(std::unique_ptr<SafetyTipInfoBarDelegate> delegate);

  // ConfirmInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;

  SafetyTipInfoBarDelegate* GetDelegate();
};

#endif  // CHROME_BROWSER_LOOKALIKES_SAFETY_TIP_INFOBAR_ANDROID_H_
