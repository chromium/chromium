// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANDATORY_REAUTH_ANDROID_MANDATORY_REAUTH_OPT_IN_VIEW_ANDROID_H_
#define CHROME_BROWSER_MANDATORY_REAUTH_ANDROID_MANDATORY_REAUTH_OPT_IN_VIEW_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/payments/mandatory_reauth_bubble_controller.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

// The native class responsible for managing the Android view to show the
// Mandatory Reauth opt-in prompt.
class MandatoryReauthOptInViewAndroid final : public AutofillBubbleBase {
 public:
  // Factory function for creating and showing the view.
  static std::unique_ptr<MandatoryReauthOptInViewAndroid> CreateAndShow(
      content::WebContents* web_contents,
      MandatoryReauthBubbleController* controller);

  MandatoryReauthOptInViewAndroid();
  ~MandatoryReauthOptInViewAndroid();
  MandatoryReauthOptInViewAndroid(const MandatoryReauthOptInViewAndroid&) =
      delete;
  MandatoryReauthOptInViewAndroid& operator=(
      const MandatoryReauthOptInViewAndroid&) = delete;

  // AutofillBubbleBase:
  void Hide() override;

 private:
  bool Show(content::WebContents* web_contents,
            MandatoryReauthBubbleController* controller);

  // This class's corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_view_bridge_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_MANDATORY_REAUTH_ANDROID_MANDATORY_REAUTH_OPT_IN_VIEW_ANDROID_H_
