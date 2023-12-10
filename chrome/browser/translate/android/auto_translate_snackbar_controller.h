// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_ANDROID_AUTO_TRANSLATE_SNACKBAR_CONTROLLER_H_
#define CHROME_BROWSER_TRANSLATE_ANDROID_AUTO_TRANSLATE_SNACKBAR_CONTROLLER_H_

#include <jni.h>

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}

namespace translate {

class TranslateManager;

// The AutoTranslateSnackbarController coordinates between the
// ChromeTranslateClient and the Java SnackbarManager to show a Snackbar when
// auto-translations occur.  An AutoTranslateSnackbarController is created the
// first time an auto-translation is shown in a tab and destroyed when the tab
// is destroyed.
class AutoTranslateSnackbarController {
 public:
  AutoTranslateSnackbarController(
      content::WebContents* web_contents,
      const base::WeakPtr<TranslateManager>& translate_manager);

  AutoTranslateSnackbarController(const AutoTranslateSnackbarController&) =
      delete;
  AutoTranslateSnackbarController& operator=(
      const AutoTranslateSnackbarController&) = delete;

  // Dismiss Snackbar on destruction if it is showing.
  ~AutoTranslateSnackbarController();

  // Show the auto-translate snackbar for the given target language.
  void ShowSnackbar(const std::string& target_langage);

  // Whether or not the auto-translate snackbar is currently showing.
  bool IsShowing();

  // Called by Java when the snackbar is dismissed with no action.
  void OnDismissNoAction(JNIEnv* env);

  // Called by Java when the Undo action is pressed.
  void OnUndoActionPressed(
      JNIEnv* env,
      base::android::JavaParamRef<jstring> target_language);

  // Called by native to manually dismiss the snackbar
  void NativeDismissSnackbar();

  // Passes on JNI calls to the stored Java AutoTranslateSnackbarController.
  // This interface exists in order to make it easier to test
  // AutoTranslateSnackbarController.
  class Bridge {
   public:
    virtual ~Bridge();

    virtual bool CreateAutoTranslateSnackbarController(
        JNIEnv* env,
        content::WebContents* web_contents,
        AutoTranslateSnackbarController* native_auto_translate_snackbar) = 0;

    virtual void ShowSnackbar(
        JNIEnv* env,
        base::android::ScopedJavaLocalRef<jstring> target_language) = 0;
    virtual bool CanShowSnackbar() = 0;
    virtual void WasDismissed() = 0;
    virtual bool IsSnackbarShowing() = 0;

    virtual void DismissSnackbar(JNIEnv* env) = 0;
  };

  // Test-only constructor to enable a custom Bridge.
  AutoTranslateSnackbarController(
      content::WebContents* web_contents,
      const base::WeakPtr<TranslateManager>& translate_manager,
      std::unique_ptr<Bridge> bridge);

 private:
  raw_ptr<content::WebContents> web_contents_;
  base::WeakPtr<TranslateManager> translate_manager_;
  base::android::ScopedJavaGlobalRef<jobject> java_auto_translate_snackbar_;
  std::unique_ptr<Bridge> bridge_;
};

}  // namespace translate

#endif  // CHROME_BROWSER_TRANSLATE_ANDROID_AUTO_TRANSLATE_SNACKBAR_CONTROLLER_H_
