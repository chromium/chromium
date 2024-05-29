// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_blocked_dialog_controller_android.h"

#include "base/android/jni_string.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/PermissionBlockedDialog_jni.h"

namespace {

using PrimaryButtonBehavior =
    QuietPermissionPromptModelAndroid::PrimaryButtonBehavior;
using SecondaryButtonBehavior =
    QuietPermissionPromptModelAndroid::SecondaryButtonBehavior;

}  // namespace

PermissionBlockedDialogController::PermissionBlockedDialogController(
    Delegate* delegate,
    content::WebContents* web_contents)
    : delegate_(delegate), web_contents_(web_contents) {}

PermissionBlockedDialogController::~PermissionBlockedDialogController() {
  DismissDialog();
}

void PermissionBlockedDialogController::ShowDialog(
    permissions::PermissionUiSelector::QuietUiReason quiet_ui_reason) {
  if (!GetOrCreateJavaObject()) {
    return;
  }

  prompt_model_ = GetQuietPermissionPromptModel(
      quiet_ui_reason, delegate_->GetContentSettingsType());
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PermissionBlockedDialog_show(
      env, GetOrCreateJavaObject(), prompt_model_.title,
      prompt_model_.description, prompt_model_.primary_button_label,
      prompt_model_.secondary_button_label, prompt_model_.learn_more_text);
}

void PermissionBlockedDialogController::OnPrimaryButtonClicked(JNIEnv* env) {
  switch (prompt_model_.primary_button_behavior) {
    case PrimaryButtonBehavior::kAllowForThisSite:
      delegate_->OnAllowForThisSite();
      return;
    case PrimaryButtonBehavior::kContinueBlocking:
      delegate_->OnContinueBlocking();
      return;
  }
}

void PermissionBlockedDialogController::OnNegativeButtonClicked(JNIEnv* env) {
  switch (prompt_model_.secondary_button_behavior) {
    case SecondaryButtonBehavior::kShowSettings:
      delegate_->OnOpenedSettings();
      Java_PermissionBlockedDialog_showSettings(env, GetOrCreateJavaObject(), static_cast<int>(delegate_->GetContentSettingsType()));
      return;
    case SecondaryButtonBehavior::kAllowForThisSite:
      delegate_->OnAllowForThisSite();
      return;
  }
}

void PermissionBlockedDialogController::OnLearnMoreClicked(JNIEnv* env) {
  delegate_->OnLearnMoreClicked();
}

void PermissionBlockedDialogController::OnDialogDismissed(JNIEnv* env) {
  java_object_.Reset();
  delegate_->OnDialogDismissed();
}

void PermissionBlockedDialogController::DismissDialog() {
  if (java_object_) {
    Java_PermissionBlockedDialog_dismissDialog(
        base::android::AttachCurrentThread(), java_object_);
  }
}

base::android::ScopedJavaGlobalRef<jobject>
PermissionBlockedDialogController::GetOrCreateJavaObject() {
  if (java_object_) {
    return java_object_;
  }

  if (web_contents_->GetNativeView() == nullptr ||
      web_contents_->GetNativeView()->GetWindowAndroid() == nullptr) {
    return nullptr;  // No window attached (yet or anymore).
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = web_contents_->GetNativeView();
  return java_object_ = Java_PermissionBlockedDialog_create(
             env, reinterpret_cast<intptr_t>(this),
             view_android->GetWindowAndroid()->GetJavaObject());
}
