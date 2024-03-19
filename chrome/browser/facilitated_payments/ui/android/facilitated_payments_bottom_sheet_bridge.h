// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FACILITATED_PAYMENTS_UI_ANDROID_FACILITATED_PAYMENTS_BOTTOM_SHEET_BRIDGE_H_
#define CHROME_BROWSER_FACILITATED_PAYMENTS_UI_ANDROID_FACILITATED_PAYMENTS_BOTTOM_SHEET_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "ui/android/window_android.h"

namespace content {
class WebContents;
}  // namespace content

namespace payments::facilitated {

// Bridge class providing an entry point to trigger the facilitated payments
// bottom sheet on Android.
class FacilitatedPaymentsBottomSheetBridge {
 public:
  FacilitatedPaymentsBottomSheetBridge();

  FacilitatedPaymentsBottomSheetBridge(
      const FacilitatedPaymentsBottomSheetBridge&) = delete;
  FacilitatedPaymentsBottomSheetBridge& operator=(
      const FacilitatedPaymentsBottomSheetBridge&) = delete;

  ~FacilitatedPaymentsBottomSheetBridge();

  bool RequestShowContent(content::WebContents* web_contents);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;
};

}  // namespace payments::facilitated

#endif  // CHROME_BROWSER_FACILITATED_PAYMENTS_UI_ANDROID_FACILITATED_PAYMENTS_BOTTOM_SHEET_BRIDGE_H_
