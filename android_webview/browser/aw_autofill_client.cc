// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_autofill_client.h"

#include <utility>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_contents.h"
#include "android_webview/browser/aw_form_database_service.h"
#include "android_webview/browser_jni_headers/AwAutofillClient_jni.h"
#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/ui/autofill_popup_delegate.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/gfx/geometry/rect_f.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::WebContents;

namespace android_webview {

AwAutofillClient::~AwAutofillClient() {
  HideAutofillPopup(autofill::PopupHidingReason::kTabGone);
}

void AwAutofillClient::SetSaveFormData(bool enabled) {
  save_form_data_ = enabled;
}

bool AwAutofillClient::GetSaveFormData() const {
  return save_form_data_;
}

autofill::PersonalDataManager* AwAutofillClient::GetPersonalDataManager() {
  return nullptr;
}

autofill::AutocompleteHistoryManager*
AwAutofillClient::GetAutocompleteHistoryManager() {
  return AwBrowserContext::FromWebContents(&GetWebContents())
      ->GetAutocompleteHistoryManager();
}

PrefService* AwAutofillClient::GetPrefs() {
  return const_cast<PrefService*>(std::as_const(*this).GetPrefs());
}

const PrefService* AwAutofillClient::GetPrefs() const {
  return user_prefs::UserPrefs::Get(AwBrowserContext::FromWebContents(
      const_cast<WebContents*>(&GetWebContents())));
}

syncer::SyncService* AwAutofillClient::GetSyncService() {
  return nullptr;
}

signin::IdentityManager* AwAutofillClient::GetIdentityManager() {
  return nullptr;
}

autofill::FormDataImporter* AwAutofillClient::GetFormDataImporter() {
  return nullptr;
}

autofill::payments::PaymentsClient* AwAutofillClient::GetPaymentsClient() {
  return nullptr;
}

autofill::StrikeDatabase* AwAutofillClient::GetStrikeDatabase() {
  return nullptr;
}

ukm::UkmRecorder* AwAutofillClient::GetUkmRecorder() {
  return nullptr;
}

ukm::SourceId AwAutofillClient::GetUkmSourceId() {
  // UKM recording is not supported for WebViews.
  return ukm::kInvalidSourceId;
}

autofill::AddressNormalizer* AwAutofillClient::GetAddressNormalizer() {
  return nullptr;
}

const GURL& AwAutofillClient::GetLastCommittedPrimaryMainFrameURL() const {
  return GetWebContents().GetPrimaryMainFrame()->GetLastCommittedURL();
}

url::Origin AwAutofillClient::GetLastCommittedPrimaryMainFrameOrigin() const {
  return GetWebContents().GetPrimaryMainFrame()->GetLastCommittedOrigin();
}

security_state::SecurityLevel
AwAutofillClient::GetSecurityLevelForUmaHistograms() {
  // The metrics are not recorded for Android webview, so return the count value
  // which will not be recorded.
  return security_state::SecurityLevel::SECURITY_LEVEL_COUNT;
}

const translate::LanguageState* AwAutofillClient::GetLanguageState() {
  return nullptr;
}

translate::TranslateDriver* AwAutofillClient::GetTranslateDriver() {
  return nullptr;
}

void AwAutofillClient::ShowAutofillSettings(bool show_credit_card_settings) {
  NOTIMPLEMENTED();
}

void AwAutofillClient::ShowUnmaskPrompt(
    const autofill::CreditCard& card,
    const autofill::CardUnmaskPromptOptions& card_unmask_prompt_options,
    base::WeakPtr<autofill::CardUnmaskDelegate> delegate) {
  NOTIMPLEMENTED();
}

void AwAutofillClient::OnUnmaskVerificationResult(PaymentsRpcResult result) {
  NOTIMPLEMENTED();
}

void AwAutofillClient::ConfirmAccountNameFixFlow(
    base::OnceCallback<void(const std::u16string&)> callback) {
  NOTIMPLEMENTED();
}

void AwAutofillClient::ConfirmExpirationDateFixFlow(
    const autofill::CreditCard& card,
    base::OnceCallback<void(const std::u16string&, const std::u16string&)>
        callback) {
  NOTIMPLEMENTED();
}

void AwAutofillClient::ConfirmSaveCreditCardLocally(
    const autofill::CreditCard& card,
    SaveCreditCardOptions options,
    LocalSaveCardPromptCallback callback) {
  NOTIMPLEMENTED();
}

void AwAutofillClient::ConfirmSaveCreditCardToCloud(
    const autofill::CreditCard& card,
    const autofill::LegalMessageLines& legal_message_lines,
    SaveCreditCardOptions options,
    UploadSaveCardPromptCallback callback) {
  NOTIMPLEMENTED();
}

void AwAutofillClient::CreditCardUploadCompleted(bool card_saved) {
  NOTIMPLEMENTED();
}

void AwAutofillClient::ConfirmCreditCardFillAssist(
    const autofill::CreditCard& card,
    base::OnceClosure callback) {
  NOTIMPLEMENTED();
}

void AwAutofillClient::ConfirmSaveAddressProfile(
    const autofill::AutofillProfile& profile,
    const autofill::AutofillProfile* original_profile,
    SaveAddressProfilePromptOptions options,
    AddressProfileSavePromptCallback callback) {
  NOTIMPLEMENTED();
}

bool AwAutofillClient::HasCreditCardScanFeature() {
  return false;
}

void AwAutofillClient::ScanCreditCard(CreditCardScanCallback callback) {
  NOTIMPLEMENTED();
}

bool AwAutofillClient::IsFastCheckoutSupported() {
  return false;
}

bool AwAutofillClient::IsFastCheckoutTriggerForm(
    const autofill::FormData& form,
    const autofill::FormFieldData& field) {
  return false;
}

bool AwAutofillClient::ShowFastCheckout(
    base::WeakPtr<autofill::FastCheckoutDelegate> delegate) {
  NOTREACHED();
  return false;
}

void AwAutofillClient::HideFastCheckout() {
  NOTREACHED();
}

bool AwAutofillClient::IsTouchToFillCreditCardSupported() {
  return false;
}

bool AwAutofillClient::ShowTouchToFillCreditCard(
    base::WeakPtr<autofill::TouchToFillDelegate> delegate,
    base::span<const autofill::CreditCard* const> cards_to_suggest) {
  NOTREACHED();
  return false;
}

void AwAutofillClient::HideTouchToFillCreditCard() {
  NOTREACHED();
}

void AwAutofillClient::ShowAutofillPopup(
    const autofill::AutofillClient::PopupOpenArgs& open_args,
    base::WeakPtr<autofill::AutofillPopupDelegate> delegate) {
  suggestions_ = open_args.suggestions;
  delegate_ = delegate;

  // Convert element_bounds to be in screen space.
  gfx::Rect client_area = GetWebContents().GetContainerBounds();
  gfx::RectF element_bounds_in_screen_space =
      open_args.element_bounds + client_area.OffsetFromOrigin();

  ShowAutofillPopupImpl(element_bounds_in_screen_space,
                        open_args.text_direction == base::i18n::RIGHT_TO_LEFT,
                        open_args.suggestions);
}

void AwAutofillClient::UpdateAutofillPopupDataListValues(
    const std::vector<std::u16string>& values,
    const std::vector<std::u16string>& labels) {
  // Leaving as an empty method since updating autofill popup window
  // dynamically does not seem to be a useful feature for android webview.
  // See crrev.com/18102002 if need to implement.
}

base::span<const autofill::Suggestion> AwAutofillClient::GetPopupSuggestions()
    const {
  NOTIMPLEMENTED();
  return base::span<const autofill::Suggestion>();
}

void AwAutofillClient::PinPopupView() {
  NOTIMPLEMENTED();
}

autofill::AutofillClient::PopupOpenArgs AwAutofillClient::GetReopenPopupArgs()
    const {
  NOTIMPLEMENTED();
  return {};
}

void AwAutofillClient::UpdatePopup(
    const std::vector<autofill::Suggestion>& suggestions,
    autofill::PopupType popup_type) {
  NOTIMPLEMENTED();
}

void AwAutofillClient::HideAutofillPopup(autofill::PopupHidingReason reason) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return;
  delegate_.reset();
  Java_AwAutofillClient_hideAutofillPopup(env, obj);
}

bool AwAutofillClient::IsAutocompleteEnabled() const {
  return GetSaveFormData();
}

bool AwAutofillClient::IsPasswordManagerEnabled() {
  // Android O+ relies on the AndroidAutofillManager, which does not call this
  // function. If it ever does, the function needs to be implemented in a
  // meaningful way.
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_OREO) {
    NOTREACHED();
  }
  // This is behavior preserving: For pre-O versions, AwAutofill did rely on a
  // BrowserAutofillManager, which now calls the function. But pre-O only
  // offered an autocomplete feature that restored values of specific input
  // elements. It did not support password management.
  return false;
}

void AwAutofillClient::PropagateAutofillPredictions(
    autofill::AutofillDriver* driver,
    const std::vector<autofill::FormStructure*>& forms) {}

void AwAutofillClient::DidFillOrPreviewField(
    const std::u16string& autofilled_value,
    const std::u16string& profile_full_name) {}

bool AwAutofillClient::IsContextSecure() const {
  content::SSLStatus ssl_status;
  content::NavigationEntry* navigation_entry =
      GetWebContents().GetController().GetLastCommittedEntry();
  if (!navigation_entry)
    return false;

  ssl_status = navigation_entry->GetSSL();
  // Note: As of crbug.com/701018, Chrome relies on SecurityStateTabHelper to
  // determine whether the page is secure, but WebView has no equivalent class.

  return navigation_entry->GetURL().SchemeIsCryptographic() &&
         ssl_status.certificate &&
         !net::IsCertStatusError(ssl_status.cert_status) &&
         !(ssl_status.content_status &
           content::SSLStatus::RAN_INSECURE_CONTENT);
}

bool AwAutofillClient::ShouldShowSigninPromo() {
  return false;
}

bool AwAutofillClient::AreServerCardsSupported() const {
  return true;
}

void AwAutofillClient::ExecuteCommand(int id) {
  NOTIMPLEMENTED();
}

void AwAutofillClient::OpenPromoCodeOfferDetailsURL(const GURL& url) {
  NOTIMPLEMENTED();
}

autofill::FormInteractionsFlowId
AwAutofillClient::GetCurrentFormInteractionsFlowId() {
  // Currently not in use here. See `ChromeAutofillClient` for a proper
  // implementation.
  return {};
}

void AwAutofillClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  NOTIMPLEMENTED();
}

void AwAutofillClient::Dismissed(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj) {
  anchor_view_.Reset();
}

void AwAutofillClient::SuggestionSelected(JNIEnv* env,
                                          const JavaParamRef<jobject>& object,
                                          jint position) {
  if (delegate_) {
    delegate_->DidAcceptSuggestion(suggestions_[position], position);
  }
}

// Ownership: The native object is created (if autofill enabled) and owned by
// AwContents. The native object creates the java peer which handles most
// autofill functionality at the java side. The java peer is owned by Java
// AwContents. The native object only maintains a weak ref to it.
AwAutofillClient::AwAutofillClient(WebContents* contents)
    : content::WebContentsUserData<AwAutofillClient>(*contents) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> delegate;
  delegate.Reset(
      Java_AwAutofillClient_create(env, reinterpret_cast<intptr_t>(this)));

  AwContents* aw_contents = AwContents::FromWebContents(contents);
  aw_contents->SetAwAutofillClient(delegate);
  java_ref_ = JavaObjectWeakGlobalRef(env, delegate);
}

void AwAutofillClient::ShowAutofillPopupImpl(
    const gfx::RectF& element_bounds,
    bool is_rtl,
    const std::vector<autofill::Suggestion>& suggestions) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return;

  // We need an array of AutofillSuggestion.
  size_t count = suggestions.size();

  ScopedJavaLocalRef<jobjectArray> data_array =
      Java_AwAutofillClient_createAutofillSuggestionArray(env, count);

  for (size_t i = 0; i < count; ++i) {
    ScopedJavaLocalRef<jstring> name =
        ConvertUTF16ToJavaString(env, suggestions[i].main_text.value);
    ScopedJavaLocalRef<jstring> label =
        base::android::ConvertUTF8ToJavaString(env, std::string());
    // For Android, we only show the primary/first label in the matrix.
    if (!suggestions[i].labels.empty())
      label = ConvertUTF16ToJavaString(env, suggestions[i].labels[0][0].value);

    Java_AwAutofillClient_addToAutofillSuggestionArray(
        env, data_array, i, name, label, suggestions[i].frontend_id);
  }
  ui::ViewAndroid* view_android = GetWebContents().GetNativeView();
  if (!view_android)
    return;

  const ScopedJavaLocalRef<jobject> current_view = anchor_view_.view();
  if (!current_view)
    anchor_view_ = view_android->AcquireAnchorView();

  const ScopedJavaLocalRef<jobject> view = anchor_view_.view();
  if (!view)
    return;

  view_android->SetAnchorRect(view, element_bounds);
  Java_AwAutofillClient_showAutofillPopup(env, obj, view, is_rtl, data_array);
}

content::WebContents& AwAutofillClient::GetWebContents() const {
  // While a const_cast is not ideal. The Autofill API uses const in various
  // spots and the content public API doesn't have const accessors. So the const
  // cast is the lesser of two evils.
  return const_cast<content::WebContents&>(
      content::WebContentsUserData<AwAutofillClient>::GetWebContents());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AwAutofillClient);

}  // namespace android_webview
