// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "chrome/browser/translate/android/auto_translate_snackbar_controller.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_metrics_logger.h"
#include "components/translate/core/common/translate_metrics.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/translate/android/jni_headers/AutoTranslateSnackbarController_jni.h"

namespace translate {

namespace {

// Default implementation of the AutoTranslateSnackbarController::Bridge
// interface, which just calls the appropriate Java methods in each case.
class BridgeImpl : public AutoTranslateSnackbarController::Bridge {
 public:
  ~BridgeImpl() override;

  bool CreateAutoTranslateSnackbarController(
      JNIEnv* env,
      content::WebContents* web_contents,
      AutoTranslateSnackbarController* native_auto_translate_snackbar)
      override {
    CHECK(!java_auto_translate_snackbar_controller_);
    java_auto_translate_snackbar_controller_ =
        Java_AutoTranslateSnackbarController_create(
            env, web_contents->GetJavaWebContents(),
            reinterpret_cast<intptr_t>(native_auto_translate_snackbar));
    return bool(java_auto_translate_snackbar_controller_);
  }

  void ShowSnackbar(
      JNIEnv* env,
      base::android::ScopedJavaLocalRef<jstring> target_language) override {
    if (!CanShowSnackbar()) {
      return;
    }
    Java_AutoTranslateSnackbarController_show(
        env, java_auto_translate_snackbar_controller_,
        std::move(target_language));
    is_showing_ = true;
  }

  bool CanShowSnackbar() override {
    return bool(java_auto_translate_snackbar_controller_);
  }

  void WasDismissed() override { is_showing_ = false; }

  bool IsSnackbarShowing() override { return is_showing_; }

  void DismissSnackbar(JNIEnv* env) override {
    if (!CanShowSnackbar()) {
      return;
    }
    Java_AutoTranslateSnackbarController_dismiss(
        env, java_auto_translate_snackbar_controller_);
    is_showing_ = false;
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject>
      java_auto_translate_snackbar_controller_;
  bool is_showing_;
};

BridgeImpl::~BridgeImpl() = default;

}  // namespace

AutoTranslateSnackbarController::Bridge::~Bridge() = default;

AutoTranslateSnackbarController::AutoTranslateSnackbarController(
    content::WebContents* web_contents,
    const base::WeakPtr<TranslateManager>& translate_manager,
    std::unique_ptr<Bridge> bridge)
    : web_contents_(web_contents),
      translate_manager_(translate_manager),
      bridge_(std::move(bridge)) {}

AutoTranslateSnackbarController::AutoTranslateSnackbarController(
    content::WebContents* web_contents,
    const base::WeakPtr<TranslateManager>& translate_manager)
    : AutoTranslateSnackbarController(web_contents,
                                      translate_manager,
                                      std::make_unique<BridgeImpl>()) {
}

AutoTranslateSnackbarController::~AutoTranslateSnackbarController() {
  JNIEnv* env = base::android::AttachCurrentThread();
  bridge_->DismissSnackbar(env);
}

void AutoTranslateSnackbarController::ShowSnackbar(
    const std::string& target_language) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> java_target_langauge =
      base::android::ConvertUTF8ToJavaString(env, target_language);
  // If this is the first time ShowSnackbar has been called or the Java
  // AutoTranslateSnackbar failed to be created last time, create it.
  if (!bridge_->CanShowSnackbar() &&
      !bridge_->CreateAutoTranslateSnackbarController(env, web_contents_,
                                                      this)) {
    // The Java AutoTranslateSnackbar failed to create, such as
    // when the activity is being destroyed, so there is no Snackbar to show.
    return;
  }
  bridge_->ShowSnackbar(env, java_target_langauge);
}

bool AutoTranslateSnackbarController::IsShowing() {
  return bridge_->IsSnackbarShowing();
}

void AutoTranslateSnackbarController::OnDismissNoAction(JNIEnv* env) {
  bridge_->WasDismissed();
}

void AutoTranslateSnackbarController::OnUndoActionPressed(
    JNIEnv* env,
    base::android::JavaParamRef<jstring> j_target_language) {
  const std::string target_language =
      base::android::ConvertJavaStringToUTF8(env, j_target_language);

  ReportCompactInfobarEvent(InfobarEvent::INFOBAR_REVERT);
  translate_manager_->GetActiveTranslateMetricsLogger()->LogUIInteraction(
      UIInteraction::kRevert);
  translate_manager_->RevertTranslation();
  translate_manager_->ShowTranslateUI(target_language,
                                      /* auto_translate */ false,
                                      /* triggered_from_menu */ false);
  bridge_->WasDismissed();
}

void AutoTranslateSnackbarController::NativeDismissSnackbar() {
  JNIEnv* env = base::android::AttachCurrentThread();
  bridge_->DismissSnackbar(env);
}

}  // namespace translate
