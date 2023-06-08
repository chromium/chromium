// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/autofill/test/jni_headers/AutofillTestHelper_jni.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"

namespace autofill {

void JNI_AutofillTestHelper_DisableThresholdForCurrentlyShownAutofillPopup(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  base::WeakPtr<AutofillPopupControllerImpl> popup_controller =
      ChromeAutofillClient::FromWebContentsForTesting(web_contents)
          ->popup_controller_for_testing();
  if (popup_controller) {
    popup_controller->DisableThresholdForTesting(/*disable_threshold=*/true);
  }
}

}  // namespace autofill
