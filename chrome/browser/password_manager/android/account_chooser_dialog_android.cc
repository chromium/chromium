// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/account_chooser_dialog_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/password_manager/android/credential_android.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/account_avatar_fetcher.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/android/java_bitmap.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/AccountChooserDialog_jni.h"

namespace {

void JNI_AccountChooserDialog_AddElementsToJavaCredentialArray(
    JNIEnv* env,
    base::android::ScopedJavaLocalRef<jobjectArray> java_credentials_array,
    const std::vector<std::unique_ptr<password_manager::PasswordForm>>& forms) {
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

  AvatarFetcherAndroid(const AvatarFetcherAndroid&) = delete;
  AvatarFetcherAndroid& operator=(const AvatarFetcherAndroid&) = delete;

 private:
  ~AvatarFetcherAndroid() override = default;

  // chrome::BitmapFetcherDelegate:
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override;

  int index_;
  base::android::ScopedJavaGlobalRef<jobject> java_dialog_;
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
        gfx::ConvertToJavaBitmap(*bitmap);
    Java_AccountChooserDialog_imageFetchComplete(
        base::android::AttachCurrentThread(), java_dialog_, index_,
        java_bitmap);
  }
  delete this;
}

void FetchAvatar(const base::android::ScopedJavaGlobalRef<jobject>& java_dialog,
                 const password_manager::PasswordForm* password_form,
                 int index,
                 network::mojom::URLLoaderFactory* loader_factory,
                 const url::Origin& initiator) {
  if (!password_form->icon_url.is_valid()) {
    return;
  }
  // Fetcher deletes itself once fetching is finished.
  auto* fetcher =
      new AvatarFetcherAndroid(password_form->icon_url, index, java_dialog);
  fetcher->Start(loader_factory, initiator);
}

}  // namespace

AccountChooserDialogAndroid::AccountChooserDialogAndroid(
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client,
    std::vector<std::unique_ptr<password_manager::PasswordForm>>
        local_credentials,
    const url::Origin& origin,
    ManagePasswordsState::CredentialsCallback callback)
    : content::WebContentsObserver(web_contents),
      web_contents_(web_contents),
      client_(client),
      origin_(origin) {
  DCHECK(client);
  passwords_data_.set_client(client);
  passwords_data_.OnRequestCredentials(std::move(local_credentials), origin);
  passwords_data_.set_credentials_callback(std::move(callback));
}

AccountChooserDialogAndroid::~AccountChooserDialogAndroid() {
  if (authenticator_) {
    authenticator_->Cancel();
  }

  // |dialog_jobject_| can be null in tests or if the dialog could not
  // be shown.
  if (dialog_jobject_) {
    Java_AccountChooserDialog_notifyNativeDestroyed(
        base::android::AttachCurrentThread(), dialog_jobject_);
  }
}

bool AccountChooserDialogAndroid::ShowDialog() {
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents_);
  if (!(tab && tab->IsUserInteractable())) {
    delete this;
    return false;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  std::u16string title =
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_TITLE);
  gfx::NativeWindow native_window = web_contents_->GetTopLevelNativeWindow();
  base::android::ScopedJavaLocalRef<jobjectArray> java_credentials_array =
      CreateNativeCredentialArray(env, local_credentials_forms().size());
  JNI_AccountChooserDialog_AddElementsToJavaCredentialArray(
      env, java_credentials_array, local_credentials_forms());
  base::android::ScopedJavaGlobalRef<jobject> java_dialog_global;
  const std::string origin = password_manager::GetShownOrigin(origin_);
  std::u16string signin_button;
  if (local_credentials_forms().size() == 1) {
    signin_button =
        l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_SIGN_IN);
  }
  dialog_jobject_.Reset(Java_AccountChooserDialog_createAndShowAccountChooser(
      env, native_window->GetJavaObject(), reinterpret_cast<intptr_t>(this),
      java_credentials_array, title, 0, 0, origin, signin_button));
  mojo::Remote<network::mojom::URLLoaderFactory> loader_factory =
      GetURLLoaderForMainFrame(web_contents_);
  int avatar_index = 0;
  for (const auto& form : local_credentials_forms()) {
    FetchAvatar(dialog_jobject_, form.get(), avatar_index++,
                loader_factory.get(),
                web_contents_->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  }
  return true;
}

void AccountChooserDialogAndroid::OnCredentialClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint credential_item,
    jboolean signin_button_clicked) {
  bool credential_handled =
      HandleCredentialChosen(credential_item, signin_button_clicked);
  if (credential_handled) {
    delete this;
  }
}

void AccountChooserDialogAndroid::CancelDialog(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  OnDialogCancel();
  delete this;
}

void AccountChooserDialogAndroid::OnLinkClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  web_contents_->OpenURL(
      content::OpenURLParams(
          GURL(password_manager::kPasswordManagerHelpCenterSmartLock),
          content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui::PAGE_TRANSITION_LINK, false /* is_renderer_initiated */),
      /*navigation_handle_callback=*/{});
  delete this;
}

void AccountChooserDialogAndroid::WebContentsDestroyed() {
  delete this;
}

void AccountChooserDialogAndroid::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility != content::Visibility::HIDDEN) {
    return;
  }

  // If an authentication is in progress, the user already selected a
  // credential so the dialog action should not be marked as cancel.
  if (!authenticator_) {
    OnDialogCancel();
  }
  delete this;
}

void AccountChooserDialogAndroid::OnDialogCancel() {
  passwords_data_.ChooseCredential(nullptr);
}

const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
AccountChooserDialogAndroid::local_credentials_forms() const {
  return passwords_data_.GetCurrentForms();
}

bool AccountChooserDialogAndroid::HandleCredentialChosen(
    size_t index,
    bool signin_button_clicked) {
  const auto& credentials_forms = local_credentials_forms();
  if (index >= credentials_forms.size()) {
    // There is nothing more to handle.
    return true;
  }

  std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator =
      client_->GetDeviceAuthenticator();
  if (client_->IsReauthBeforeFillingRequired(authenticator.get())) {
    authenticator_ = std::move(authenticator);
    authenticator_->AuthenticateWithMessage(
        u"", base::BindOnce(&AccountChooserDialogAndroid::OnReauthCompleted,
                            base::Unretained(this), index));
    // The credential handling will only happen after the authentication
    // finishes.
    return false;
  }

  passwords_data_.ChooseCredential(credentials_forms[index].get());
  return true;
}

void AccountChooserDialogAndroid::OnReauthCompleted(size_t index,
                                                    bool auth_succeeded) {
  authenticator_.reset();
  if (auth_succeeded) {
    const auto& credentials_forms = local_credentials_forms();
    passwords_data_.ChooseCredential(credentials_forms[index].get());
  } else {
    passwords_data_.ChooseCredential(nullptr);
  }
  delete this;
}
