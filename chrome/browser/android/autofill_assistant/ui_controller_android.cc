// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/ui_controller_android.h"

#include <map>
#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "components/autofill_assistant/browser/access_token_fetcher.h"
#include "components/autofill_assistant/browser/controller.h"
#include "components/signin/core/browser/account_info.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/google_api_keys.h"
#include "jni/AutofillAssistantUiController_jni.h"
#include "services/identity/public/cpp/identity_manager.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;

namespace autofill_assistant {
namespace switches {
const char* const kAutofillAssistantServerKey = "autofill-assistant-key";
}  // namespace switches

namespace {

// Builds a map from two Java arrays of strings with the same length.
std::unique_ptr<std::map<std::string, std::string>> BuildParametersFromJava(
    JNIEnv* env,
    const JavaRef<jobjectArray>& names,
    const JavaRef<jobjectArray>& values) {
  std::vector<std::string> names_vector;
  base::android::AppendJavaStringArrayToStringVector(env, names, &names_vector);
  std::vector<std::string> values_vector;
  base::android::AppendJavaStringArrayToStringVector(env, values,
                                                     &values_vector);
  DCHECK_EQ(names_vector.size(), values_vector.size());
  auto parameters = std::make_unique<std::map<std::string, std::string>>();
  for (size_t i = 0; i < names_vector.size(); ++i) {
    parameters->insert(std::make_pair(names_vector[i], values_vector[i]));
  }
  return parameters;
}
}  // namespace

UiControllerAndroid::UiControllerAndroid(
    JNIEnv* env,
    jobject jcaller,
    const JavaParamRef<jobject>& webContents,
    const base::android::JavaParamRef<jobjectArray>& parameterNames,
    const base::android::JavaParamRef<jobjectArray>& parameterValues,
    const base::android::JavaParamRef<jstring>& initialUrlString)
    : ui_delegate_(nullptr) {
  java_autofill_assistant_ui_controller_.Reset(env, jcaller);

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(webContents);
  DCHECK(web_contents);
  browser_context_ = web_contents->GetBrowserContext();
  GURL initialUrl =
      GURL(base::android::ConvertJavaStringToUTF8(env, initialUrlString));
  Controller::CreateAndStartForWebContents(
      web_contents, base::WrapUnique(this),
      BuildParametersFromJava(env, parameterNames, parameterValues),
      initialUrl);
  DCHECK(ui_delegate_);
}

UiControllerAndroid::~UiControllerAndroid() {}

void UiControllerAndroid::SetUiDelegate(UiDelegate* ui_delegate) {
  ui_delegate_ = ui_delegate;
}

void UiControllerAndroid::ShowStatusMessage(const std::string& message) {
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantUiController_onShowStatusMessage(
      env, java_autofill_assistant_ui_controller_,
      base::android::ConvertUTF8ToJavaString(env, message));
}

void UiControllerAndroid::ShowOverlay() {
  Java_AutofillAssistantUiController_onShowOverlay(
      AttachCurrentThread(), java_autofill_assistant_ui_controller_);
}

void UiControllerAndroid::HideOverlay() {
  Java_AutofillAssistantUiController_onHideOverlay(
      AttachCurrentThread(), java_autofill_assistant_ui_controller_);
}

void UiControllerAndroid::Shutdown() {
  Java_AutofillAssistantUiController_onShutdown(
      AttachCurrentThread(), java_autofill_assistant_ui_controller_);
}

void UiControllerAndroid::UpdateScripts(
    const std::vector<ScriptHandle>& scripts) {
  std::vector<std::string> script_paths;
  std::vector<std::string> script_names;
  bool* script_highlights = new bool[scripts.size()];
  for (size_t i = 0; i < scripts.size(); ++i) {
    const auto& script = scripts[i];
    script_paths.emplace_back(script.path);
    script_names.emplace_back(script.name);
    script_highlights[i] = script.highlight;
  }
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantUiController_onUpdateScripts(
      env, java_autofill_assistant_ui_controller_,
      base::android::ToJavaArrayOfStrings(env, script_names),
      base::android::ToJavaArrayOfStrings(env, script_paths),
      base::android::ToJavaBooleanArray(env, script_highlights,
                                        scripts.size()));
  delete[] script_highlights;
}

void UiControllerAndroid::OnScriptSelected(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jstring>& jscript_path) {
  std::string script_path;
  base::android::ConvertJavaStringToUTF8(env, jscript_path, &script_path);
  ui_delegate_->OnScriptSelected(script_path);
}

void UiControllerAndroid::OnAddressSelected(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jaddress_guid) {
  if (!address_or_card_callback_)  // possibly duplicate call
    return;

  std::string guid;
  base::android::ConvertJavaStringToUTF8(env, jaddress_guid, &guid);
  std::move(address_or_card_callback_).Run(guid);
}

void UiControllerAndroid::OnCardSelected(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jcard_guid) {
  if (!address_or_card_callback_)  // possibly duplicate call
    return;

  std::string guid;
  base::android::ConvertJavaStringToUTF8(env, jcard_guid, &guid);
  std::move(address_or_card_callback_).Run(guid);
}

void UiControllerAndroid::OnGetPaymentInformation(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean jsucceed,
    const base::android::JavaParamRef<jstring>& jcard_guid,
    const base::android::JavaParamRef<jstring>& jcard_issuer_network,
    const base::android::JavaParamRef<jstring>& jaddress_guid,
    const base::android::JavaParamRef<jstring>& jpayer_name,
    const base::android::JavaParamRef<jstring>& jpayer_phone,
    const base::android::JavaParamRef<jstring>& jpayer_email) {
  DCHECK(get_payment_information_callback_);

  std::unique_ptr<PaymentInformation> payment_info =
      std::make_unique<PaymentInformation>();
  payment_info->succeed = jsucceed;
  if (payment_info->succeed) {
    if (jcard_guid != nullptr) {
      base::android::ConvertJavaStringToUTF8(env, jcard_guid,
                                             &payment_info->card_guid);
    }
    if (jcard_issuer_network != nullptr) {
      base::android::ConvertJavaStringToUTF8(
          env, jcard_issuer_network, &payment_info->card_issuer_network);
    }
    if (jaddress_guid != nullptr) {
      base::android::ConvertJavaStringToUTF8(env, jaddress_guid,
                                             &payment_info->address_guid);
    }
    if (jpayer_name != nullptr) {
      base::android::ConvertJavaStringToUTF8(env, jpayer_name,
                                             &payment_info->payer_name);
    }
    if (jpayer_phone != nullptr) {
      base::android::ConvertJavaStringToUTF8(env, jpayer_phone,
                                             &payment_info->payer_phone);
    }
    if (jpayer_email != nullptr) {
      base::android::ConvertJavaStringToUTF8(env, jpayer_email,
                                             &payment_info->payer_email);
    }
  }
  std::move(get_payment_information_callback_).Run(std::move(payment_info));
}

void UiControllerAndroid::OnAccessToken(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean success,
    const JavaParamRef<jstring>& access_token) {
  if (fetch_access_token_callback_) {
    std::move(fetch_access_token_callback_)
        .Run(success, base::android::ConvertJavaStringToUTF8(access_token));
  }
}

base::android::ScopedJavaLocalRef<jstring>
UiControllerAndroid::GetPrimaryAccountName(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  AccountInfo account_info = IdentityManagerFactory::GetForProfile(
                                 Profile::FromBrowserContext(browser_context_))
                                 ->GetPrimaryAccountInfo();
  return base::android::ConvertUTF8ToJavaString(env, account_info.email);
}

void UiControllerAndroid::ChooseAddress(
    base::OnceCallback<void(const std::string&)> callback) {
  DCHECK(!address_or_card_callback_);
  address_or_card_callback_ = std::move(callback);
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantUiController_onChooseAddress(
      env, java_autofill_assistant_ui_controller_);
}

void UiControllerAndroid::ChooseCard(
    base::OnceCallback<void(const std::string&)> callback) {
  DCHECK(!address_or_card_callback_);
  address_or_card_callback_ = std::move(callback);
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantUiController_onChooseCard(
      env, java_autofill_assistant_ui_controller_);
}

void UiControllerAndroid::GetPaymentInformation(
    payments::mojom::PaymentOptionsPtr payment_options,
    base::OnceCallback<void(std::unique_ptr<PaymentInformation>)> callback,
    const std::string& title) {
  DCHECK(!get_payment_information_callback_);
  get_payment_information_callback_ = std::move(callback);
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantUiController_onRequestPaymentInformation(
      env, java_autofill_assistant_ui_controller_,
      payment_options->request_shipping, payment_options->request_payer_name,
      payment_options->request_payer_phone,
      payment_options->request_payer_email,
      static_cast<int>(payment_options->shipping_type),
      base::android::ConvertUTF8ToJavaString(env, title));
}

void UiControllerAndroid::HideDetails() {
  Java_AutofillAssistantUiController_onHideDetails(
      AttachCurrentThread(), java_autofill_assistant_ui_controller_);
}

void UiControllerAndroid::ShowDetails(const DetailsProto& details) {
  int year = details.datetime().date().year();
  int month = details.datetime().date().month();
  int day = details.datetime().date().day();
  int hour = details.datetime().time().hour();
  int minute = details.datetime().time().minute();
  int second = details.datetime().time().second();

  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantUiController_onShowDetails(
      env, java_autofill_assistant_ui_controller_,
      base::android::ConvertUTF8ToJavaString(env, details.title()),
      base::android::ConvertUTF8ToJavaString(env, details.url()),
      base::android::ConvertUTF8ToJavaString(env, details.description()), year,
      month, day, hour, minute, second);
}

void UiControllerAndroid::ShowProgressBar(int progress,
                                          const std::string& message) {
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantUiController_onShowProgressBar(
      env, java_autofill_assistant_ui_controller_, progress,
      base::android::ConvertUTF8ToJavaString(env, message));
}

void UiControllerAndroid::HideProgressBar() {
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantUiController_onHideProgressBar(
      env, java_autofill_assistant_ui_controller_);
}

std::string UiControllerAndroid::GetApiKey() {
  std::string api_key;
  if (google_apis::IsGoogleChromeAPIKeyUsed()) {
    api_key = chrome::GetChannel() == version_info::Channel::STABLE
                  ? google_apis::GetAPIKey()
                  : google_apis::GetNonStableAPIKey();
  }
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAutofillAssistantServerKey)) {
    api_key = command_line->GetSwitchValueASCII(
        switches::kAutofillAssistantServerKey);
  }
  return api_key;
}

AccessTokenFetcher* UiControllerAndroid::GetAccessTokenFetcher() {
  return this;
}

autofill::PersonalDataManager* UiControllerAndroid::GetPersonalDataManager() {
  return autofill::PersonalDataManagerFactory::GetForProfile(
      ProfileManager::GetLastUsedProfile());
}

std::string UiControllerAndroid::GetServerUrl() {
  return variations::GetVariationParamValueByFeature(
      chrome::android::kAutofillAssistant, "url");
}

UiController* UiControllerAndroid::GetUiController() {
  return this;
}

void UiControllerAndroid::FetchAccessToken(
    base::OnceCallback<void(bool, const std::string&)> callback) {
  DCHECK(!fetch_access_token_callback_);

  fetch_access_token_callback_ = std::move(callback);
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantUiController_fetchAccessToken(
      env, java_autofill_assistant_ui_controller_);
}

void UiControllerAndroid::InvalidateAccessToken(
    const std::string& access_token) {
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantUiController_invalidateAccessToken(
      env, java_autofill_assistant_ui_controller_,
      base::android::ConvertUTF8ToJavaString(env, access_token));
}

void UiControllerAndroid::Destroy(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj) {
  ui_delegate_->OnDestroy();
}

static jlong JNI_AutofillAssistantUiController_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& webContents,
    const base::android::JavaParamRef<jobjectArray>& parameterNames,
    const base::android::JavaParamRef<jobjectArray>& parameterValues,
    const base::android::JavaParamRef<jstring>& initialUrlString) {
  auto* ui_controller_android = new autofill_assistant::UiControllerAndroid(
      env, jcaller, webContents, parameterNames, parameterValues,
      initialUrlString);
  return reinterpret_cast<intptr_t>(ui_controller_android);
}

}  // namespace autofill_assistant.
