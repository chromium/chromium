// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/auto_signin_first_run_dialog_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_manager_settings_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_settings_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/AutoSigninFirstRunDialog_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;

namespace {

void MarkAutoSignInFirstRunExperienceShown(content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  password_bubble_experiment::RecordAutoSignInPromptFirstRunExperienceWasShown(
      profile->GetPrefs());
}

}  // namespace

AutoSigninFirstRunDialogAndroid::AutoSigninFirstRunDialogAndroid(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents), web_contents_(web_contents) {}

AutoSigninFirstRunDialogAndroid::~AutoSigninFirstRunDialogAndroid() {}

void AutoSigninFirstRunDialogAndroid::ShowDialog() {
  gfx::NativeWindow native_window = web_contents_->GetTopLevelNativeWindow();
  if (!native_window) {
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  std::u16string explanation = l10n_util::GetStringFUTF16(
      IDS_AUTO_SIGNIN_FIRST_RUN_TEXT,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_TITLE_BRAND));
  std::u16string message = l10n_util::GetStringUTF16(
      IsSyncingAutosignSetting(profile)
          ? IDS_AUTO_SIGNIN_FIRST_RUN_TITLE_MANY_DEVICES
          : IDS_AUTO_SIGNIN_FIRST_RUN_TITLE_LOCAL_DEVICE);
  std::u16string ok_button_text =
      l10n_util::GetStringUTF16(IDS_AUTO_SIGNIN_FIRST_RUN_OK);
  std::u16string turn_off_button_text = l10n_util::GetStringUTF16(IDS_TURN_OFF);

  base::android::ScopedJavaLocalRef<jobject> java_dialog =
      native_window->GetJavaObject();
  if (java_dialog) {
    dialog_jobject_.Reset(Java_AutoSigninFirstRunDialog_createAndShowDialog(
        env, java_dialog, reinterpret_cast<intptr_t>(this), message,
        explanation, ok_button_text, turn_off_button_text));
  }
}

void AutoSigninFirstRunDialogAndroid::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void AutoSigninFirstRunDialogAndroid::OnOkClicked(JNIEnv* env, jobject obj) {
  password_manager::metrics_util::LogAutoSigninPromoUserAction(
      password_manager::metrics_util::AUTO_SIGNIN_OK_GOT_IT);
  MarkAutoSignInFirstRunExperienceShown(web_contents_);
}

void AutoSigninFirstRunDialogAndroid::OnTurnOffClicked(JNIEnv* env,
                                                       jobject obj) {
  password_manager::metrics_util::LogAutoSigninPromoUserAction(
      password_manager::metrics_util::AUTO_SIGNIN_TURN_OFF);
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  // This dialog is not and should never be shown in incognito as it offers the
  // possibility to change user settings.
  DCHECK(!profile->IsOffTheRecord());
  password_manager::PasswordManagerSettingsService* service =
      PasswordManagerSettingsServiceFactory::GetForProfile(profile);
  service->TurnOffAutoSignIn();
  MarkAutoSignInFirstRunExperienceShown(web_contents_);
}

void AutoSigninFirstRunDialogAndroid::CancelDialog(JNIEnv* env, jobject obj) {}

void AutoSigninFirstRunDialogAndroid::OnLinkClicked(JNIEnv* env, jobject obj) {
  web_contents_->OpenURL(
      content::OpenURLParams(
          GURL(password_manager::kPasswordManagerHelpCenterSmartLock),
          content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui::PAGE_TRANSITION_LINK, false /* is_renderer_initiated */),
      /*navigation_handle_callback=*/{});
}

void AutoSigninFirstRunDialogAndroid::WebContentsDestroyed() {
  if (dialog_jobject_) {
    JNIEnv* env = AttachCurrentThread();
    Java_AutoSigninFirstRunDialog_dismissDialog(env, dialog_jobject_);
  }
}

void AutoSigninFirstRunDialogAndroid::OnVisibilityChanged(
    content::Visibility visibility) {
  if (dialog_jobject_ && visibility == content::Visibility::HIDDEN) {
    // TODO(crbug.com/41253286): once bug is fixed, this code should be
    // gone.
    JNIEnv* env = AttachCurrentThread();
    Java_AutoSigninFirstRunDialog_dismissDialog(env, dialog_jobject_);
  }
}
