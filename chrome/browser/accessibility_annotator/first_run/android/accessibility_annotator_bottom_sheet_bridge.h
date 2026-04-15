// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_ANDROID_ACCESSIBILITY_ANNOTATOR_BOTTOM_SHEET_BRIDGE_H_
#define CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_ANDROID_ACCESSIBILITY_ANNOTATOR_BOTTOM_SHEET_BRIDGE_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "components/accessibility_annotator/first_run/accessibility_annotator_first_run_types.h"

namespace content {
class WebContents;
}

namespace accessibility_annotator {

// C++ side of the JNI bridge for the Accessibility Annotator First Run Bottom
// Sheet. This class is responsible for creating and displaying the Java bottom
// sheet and routing user actions back to the caller.
class AccessibilityAnnotatorBottomSheetBridge {
 public:
  AccessibilityAnnotatorBottomSheetBridge(
      content::WebContents* web_contents,
      base::OnceCallback<void(InfoResult)> callback);

  AccessibilityAnnotatorBottomSheetBridge(
      const AccessibilityAnnotatorBottomSheetBridge&) = delete;
  AccessibilityAnnotatorBottomSheetBridge& operator=(
      const AccessibilityAnnotatorBottomSheetBridge&) = delete;

  virtual ~AccessibilityAnnotatorBottomSheetBridge();

  // Requests to display the bottom sheet.
  void Show();

  // Hides the bottom sheet.
  void Hide();

  // Called from Java when the user clicks the primary "Got it" button.
  void OnInfoAcknowledged(JNIEnv* env);

  // Called from Java when the user clicks the secondary "Manage settings"
  // button.
  void OnManageSettingsClicked(JNIEnv* env);

  // Called from Java when the user clicks the "Learn more" link in the Info
  // dialog.
  void OnLearnMoreClicked(JNIEnv* env);

  // Called from Java when the user dismissed the Info dialog.
  void OnInfoDismissed(JNIEnv* env);

 protected:
  // Helper method to call Java to show the sheet. Virtual for testing.
  // Returns true if the content was shown, false if it was suppressed (e.g. if
  // higher priority content is in the sheet).
  virtual bool PerformShowContent();

 private:
  // The Java-side bridge object.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  // Callback to run when the flow completes with a result.
  base::OnceCallback<void(InfoResult)> callback_;
};

}  // namespace accessibility_annotator

#endif  // CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_FIRST_RUN_ANDROID_ACCESSIBILITY_ANNOTATOR_BOTTOM_SHEET_BRIDGE_H_
