// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/notification_blocked_dialog_controller_android.h"

#include "chrome/android/chrome_jni_headers/NotificationBlockedDialog_jni.h"

#include "base/android/jni_string.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using PrimaryButtonBehavior =
    QuietPermissionPromptModelAndroid::PrimaryButtonBehavior;
using SecondaryButtonBehavior =
    QuietPermissionPromptModelAndroid::SecondaryButtonBehavior;

}  // namespace

NotificationBlockedDialogController::NotificationBlockedDialogController(
    Delegate* delegate,
    content::WebContents* web_contents)
    : delegate_(delegate), web_contents_(web_contents) {}

NotificationBlockedDialogController::~NotificationBlockedDialogController() {
  DismissDialog();
}

void NotificationBlockedDialogController::ShowDialog(
    permissions::PermissionUiSelector::QuietUiReason quiet_ui_reason) {
  if (!GetOrCreateJavaObject())
    return;

  prompt_model_ = GetQuietNotificationPermissionPromptModel(quiet_ui_reason);
  JNIEnv* env = base::android::AttachCurrentThread();
  auto title =
      base::android::ConvertUTF16ToJavaString(env, prompt_model_.title);
  auto description =
      base::android::ConvertUTF16ToJavaString(env, prompt_model_.description);
  auto primaryButtonLabel = base::android::ConvertUTF16ToJavaString(
      env, prompt_model_.primary_button_label);
  auto secondaryButtonLabel = base::android::ConvertUTF16ToJavaString(
      env, prompt_model_.secondary_button_label);
  base::android::ScopedJavaLocalRef<jstring> learnMoreText = nullptr;
  if (!prompt_model_.learn_more_text.empty()) {
    learnMoreText = base::android::ConvertUTF16ToJavaString(
        env, prompt_model_.learn_more_text);
  }
  Java_NotificationBlockedDialog_show(env, GetOrCreateJavaObject(), title,
                                      description, primaryButtonLabel,
                                      secondaryButtonLabel, learnMoreText);
}

void NotificationBlockedDialogController::OnPrimaryButtonClicked(JNIEnv* env) {
  switch (prompt_model_.primary_button_behavior) {
    case PrimaryButtonBehavior::kAllowForThisSite:
      delegate_->OnAllowForThisSite();
      return;
    case PrimaryButtonBehavior::kContinueBlocking:
      delegate_->OnContinueBlocking();
      return;
  }
}

void NotificationBlockedDialogController::OnNegativeButtonClicked(JNIEnv* env) {
  switch (prompt_model_.secondary_button_behavior) {
    case SecondaryButtonBehavior::kShowSettings:
      delegate_->OnOpenedSettings();
      Java_NotificationBlockedDialog_showSettings(env, GetOrCreateJavaObject());
      return;
    case SecondaryButtonBehavior::kAllowForThisSite:
      delegate_->OnAllowForThisSite();
      return;
  }
}

void NotificationBlockedDialogController::OnLearnMoreClicked(JNIEnv* env) {
  delegate_->OnLearnMoreClicked();
}

void NotificationBlockedDialogController::OnDialogDismissed(JNIEnv* env) {
  java_object_.Reset();
  delegate_->OnDialogDismissed();
}

void NotificationBlockedDialogController::DismissDialog() {
  if (java_object_) {
    Java_NotificationBlockedDialog_dismissDialog(
        base::android::AttachCurrentThread(), java_object_);
  }
}

base::android::ScopedJavaGlobalRef<jobject>
NotificationBlockedDialogController::GetOrCreateJavaObject() {
  if (java_object_)
    return java_object_;

  if (web_contents_->GetNativeView() == nullptr ||
      web_contents_->GetNativeView()->GetWindowAndroid() == nullptr)
    return nullptr;  // No window attached (yet or anymore).

  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = web_contents_->GetNativeView();
  return java_object_ = Java_NotificationBlockedDialog_create(
             env, reinterpret_cast<intptr_t>(this),
             view_android->GetWindowAndroid()->GetJavaObject());
}
