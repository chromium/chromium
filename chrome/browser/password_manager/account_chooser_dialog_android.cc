// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/account_chooser_dialog_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/android/chrome_jni_headers/AccountChooserDialog_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/credential_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/account_avatar_fetcher.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "content/public/browser/storage_partition.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/range/range.h"

namespace {

void JNI_AccountChooserDialog_AddElementsToJavaCredentialArray(
    JNIEnv* env,
    base::android::ScopedJavaLocalRef<jobjectArray> java_credentials_array,
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& forms) {
  int index = 0;
  for (const auto& password_form : forms) {
    base::android::ScopedJavaLocalRef<jobject> java_credential =
        CreateNativeCredential(env, *password_form, index);
    env->SetObjectArrayElement(java_credentials_array.obj(), index,
                               java_credential.obj());
    index++;
  }
}

class AvatarFetcherAndroid : public AccountAvatarFetcher {
 public:
  AvatarFetcherAndroid(
      const GURL& url,
      int index,
      const base::android::ScopedJavaGlobalRef<jobject>& java_dialog);

 private:
  ~AvatarFetcherAndroid() override = default;

  // chrome::BitmapFetcherDelegate:
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override;

  int index_;
  base::android::ScopedJavaGlobalRef<jobject> java_dialog_;

  DISALLOW_COPY_AND_ASSIGN(AvatarFetcherAndroid);
};

AvatarFetcherAndroid::AvatarFetcherAndroid(
    const GURL& url,
    int index,
    const base::android::ScopedJavaGlobalRef<jobject>& java_dialog)
    : AccountAvatarFetcher(url, base::WeakPtr<AccountAvatarFetcherDelegate>()),
      index_(index),
      java_dialog_(java_dialog) {}

void AvatarFetcherAndroid::OnFetchComplete(const GURL& url,
                                           const SkBitmap* bitmap) {
  if (bitmap) {
    base::android::ScopedJavaLocalRef<jobject> java_bitmap =
        gfx::ConvertToJavaBitmap(bitmap);
    Java_AccountChooserDialog_imageFetchComplete(
        base::android::AttachCurrentThread(), java_dialog_, index_,
        java_bitmap);
  }
  delete this;
}

void FetchAvatar(const base::android::ScopedJavaGlobalRef<jobject>& java_dialog,
                 const autofill::PasswordForm* password_form,
                 int index,
                 network::mojom::URLLoaderFactory* loader_factory) {
  if (!password_form->icon_url.is_valid())
    return;
  // Fetcher deletes itself once fetching is finished.
  auto* fetcher =
      new AvatarFetcherAndroid(password_form->icon_url, index, java_dialog);
  fetcher->Start(loader_factory);
}

}  // namespace

AccountChooserDialogAndroid::AccountChooserDialogAndroid(
    content::WebContents* web_contents,
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials,
    const GURL& origin,
    const ManagePasswordsState::CredentialsCallback& callback)
    : content::WebContentsObserver(web_contents),
      web_contents_(web_contents),
      origin_(origin) {
  passwords_data_.set_client(
      ChromePasswordManagerClient::FromWebContents(web_contents_));
  passwords_data_.OnRequestCredentials(std::move(local_credentials), origin);
  passwords_data_.set_credentials_callback(callback);
}

AccountChooserDialogAndroid::~AccountChooserDialogAndroid() {}

bool AccountChooserDialogAndroid::ShowDialog() {
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents_);
  if (!(tab && tab->IsUserInteractable())) {
    delete this;
    return false;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  base::string16 title =
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_TITLE);
  gfx::NativeWindow native_window = web_contents_->GetTopLevelNativeWindow();
  base::android::ScopedJavaLocalRef<jobjectArray> java_credentials_array =
      CreateNativeCredentialArray(env, local_credentials_forms().size());
  JNI_AccountChooserDialog_AddElementsToJavaCredentialArray(
      env, java_credentials_array, local_credentials_forms());
  base::android::ScopedJavaGlobalRef<jobject> java_dialog_global;
  const std::string origin = password_manager::GetShownOrigin(origin_);
  base::string16 signin_button;
  if (local_credentials_forms().size() == 1) {
    signin_button =
        l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_SIGN_IN);
  }
  dialog_jobject_.Reset(Java_AccountChooserDialog_createAndShowAccountChooser(
      env, native_window->GetJavaObject(), reinterpret_cast<intptr_t>(this),
      java_credentials_array,
      base::android::ConvertUTF16ToJavaString(env, title), 0, 0,
      base::android::ConvertUTF8ToJavaString(env, origin),
      base::android::ConvertUTF16ToJavaString(env, signin_button)));
  network::mojom::URLLoaderFactory* loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()))
          ->GetURLLoaderFactoryForBrowserProcess()
          .get();
  int avatar_index = 0;
  for (const auto& form : local_credentials_forms())
    FetchAvatar(dialog_jobject_, form.get(), avatar_index++, loader_factory);
  return true;
}

void AccountChooserDialogAndroid::OnCredentialClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint credential_item,
    jboolean signin_button_clicked) {
  ChooseCredential(
      credential_item,
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD,
      signin_button_clicked);
}

void AccountChooserDialogAndroid::Destroy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  delete this;
}

void AccountChooserDialogAndroid::CancelDialog(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  OnDialogCancel();
}

void AccountChooserDialogAndroid::OnLinkClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  web_contents_->OpenURL(content::OpenURLParams(
      GURL(password_manager::kPasswordManagerHelpCenterSmartLock),
      content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false /* is_renderer_initiated */));
}

void AccountChooserDialogAndroid::WebContentsDestroyed() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AccountChooserDialog_dismissDialog(env, dialog_jobject_);
}

void AccountChooserDialogAndroid::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN) {
    // TODO(https://crbug.com/610700): once bug is fixed, this code should be
    // gone.
    OnDialogCancel();
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AccountChooserDialog_dismissDialog(env, dialog_jobject_);
  }
}

void AccountChooserDialogAndroid::OnDialogCancel() {
  ChooseCredential(-1, password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY,
                   false /* signin_button_clicked */);
}

const std::vector<std::unique_ptr<autofill::PasswordForm>>&
AccountChooserDialogAndroid::local_credentials_forms() const {
  return passwords_data_.GetCurrentForms();
}

void AccountChooserDialogAndroid::ChooseCredential(
    size_t index,
    password_manager::CredentialType type,
    bool signin_button_clicked) {
  namespace metrics = password_manager::metrics_util;

  metrics::AccountChooserUserAction action;
  if (type == password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY) {
    passwords_data_.ChooseCredential(nullptr);
    action = metrics::ACCOUNT_CHOOSER_DISMISSED;
  } else {
    action = signin_button_clicked ? metrics::ACCOUNT_CHOOSER_SIGN_IN
                                   : metrics::ACCOUNT_CHOOSER_CREDENTIAL_CHOSEN;
    const auto& credentials_forms = local_credentials_forms();
    if (index < credentials_forms.size())
      passwords_data_.ChooseCredential(credentials_forms[index].get());
  }

  if (local_credentials_forms().size() == 1)
    metrics::LogAccountChooserUserActionOneAccount(action);
  else
    metrics::LogAccountChooserUserActionManyAccounts(action);
}
