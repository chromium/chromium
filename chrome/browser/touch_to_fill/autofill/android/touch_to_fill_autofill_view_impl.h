// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_AUTOFILL_VIEW_IMPL_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_AUTOFILL_VIEW_IMPL_H_

#include <jni.h>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_autofill_view.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

class TouchToFillAutofillController;

// Android implementation of the Touch To Fill Autofill surface.
// Uses Java counterparts to present a bottom sheet.
class TouchToFillAutofillViewImpl : public TouchToFillAutofillView {
 public:
  explicit TouchToFillAutofillViewImpl(content::WebContents* web_contents);
  TouchToFillAutofillViewImpl(const TouchToFillAutofillViewImpl&) = delete;
  TouchToFillAutofillViewImpl& operator=(const TouchToFillAutofillViewImpl&) =
      delete;
  ~TouchToFillAutofillViewImpl() override;

  // TouchToFillAutofillView:
  bool ShowPersonalContextNotice(
      TouchToFillAutofillController* controller) override;
  void Hide() override;

  // JNI methods.
  void OnNoticeAcknowledged(JNIEnv* env);
  void OnDismissed(JNIEnv* env);

 private:
  // The corresponding Java TouchToFillAutofillViewBridge.
  raw_ptr<content::WebContents> web_contents_;
};
}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_AUTOFILL_VIEW_IMPL_H_
