// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_autofill_client.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_contents.h"
#include "android_webview/browser/aw_form_database_service.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "components/autofill/core/browser/autofill_popup_delegate.h"
#include "components/autofill/core/browser/suggestion.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "jni/AwAutofillClient_jni.h"
#include "ui/android/view_android.h"
#include "ui/gfx/geometry/rect_f.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::WebContents;

namespace android_webview {

// Ownership: The native object is created (if autofill enabled) and owned by
// AwContents. The native object creates the java peer which handles most
// autofill functionality at the java side. The java peer is owned by Java
// AwContents. The native object only maintains a weak ref to it.
AwAutofillClient::AwAutofillClient(WebContents* contents)
    : web_contents_(contents) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> delegate;
  delegate.Reset(
      Java_AwAutofillClient_create(env, reinterpret_cast<intptr_t>(this)));

  AwContents* aw_contents = AwContents::FromWebContents(web_contents_);
  aw_contents->SetAwAutofillClient(delegate);
  java_ref_ = JavaObjectWeakGlobalRef(env, delegate);
}

AwAutofillClient::~AwAutofillClient() {
  HideAutofillPopup();
}

void AwAutofillClient::SetSaveFormData(bool enabled) {
  save_form_data_ = enabled;
}

bool AwAutofillClient::GetSaveFormData() {
  return save_form_data_;
}

PrefService* AwAutofillClient::GetPrefs() {
  return user_prefs::UserPrefs::Get(
      AwContentBrowserClient::GetAwBrowserContext());
}

syncer::SyncService* AwAutofillClient::GetSyncService() {
  return nullptr;
}

identity::IdentityManager* AwAutofillClient::GetIdentityManager() {
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

security_state::SecurityLevel
AwAutofillClient::GetSecurityLevelForUmaHistograms() {
  // The metrics are not recorded for Android webview, so return the count value
  // which will not be recorded.
  return security_state::SecurityLevel::SECURITY_LEVEL_COUNT;
}

autofill::PersonalDataManager* AwAutofillClient::GetPersonalDataManager() {
  return nullptr;
}

scoped_refptr<autofill::AutofillWebDataService>
AwAutofillClient::GetDatabase() {
  android_webview::AwFormDatabaseService* service =
      static_cast<android_webview::AwBrowserContext*>(
          web_contents_->GetBrowserContext())
          ->GetFormDatabaseService();
  return service->get_autofill_webdata_service();
}

void AwAutofillClient::ShowAutofillPopup(
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction,
    const std::vector<autofill::Suggestion>& suggestions,
    bool /*unused_autoselect_first_suggestion*/,
    base::WeakPtr<autofill::AutofillPopupDelegate> delegate) {
  suggestions_ = suggestions;
  delegate_ = delegate;

  // Convert element_bounds to be in screen space.
  gfx::Rect client_area = web_contents_->GetContainerBounds();
  gfx::RectF element_bounds_in_screen_space =
      element_bounds + client_area.OffsetFromOrigin();

  ShowAutofillPopupImpl(element_bounds_in_screen_space,
                        text_direction == base::i18n::RIGHT_TO_LEFT,
                        suggestions);
}

void AwAutofillClient::ShowAutofillPopupImpl(
    const gfx::RectF& element_bounds,
    bool is_rtl,
    const std::vector<autofill::Suggestion>& suggestions) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  // We need an array of AutofillSuggestion.
  size_t count = suggestions.size();

  ScopedJavaLocalRef<jobjectArray> data_array =
      Java_AwAutofillClient_createAutofillSuggestionArray(env, count);

  for (size_t i = 0; i < count; ++i) {
    ScopedJavaLocalRef<jstring> name =
        ConvertUTF16ToJavaString(env, suggestions[i].value);
    ScopedJavaLocalRef<jstring> label =
        ConvertUTF16ToJavaString(env, suggestions[i].label);
    Java_AwAutofillClient_addToAutofillSuggestionArray(
        env, data_array, i, name, label, suggestions[i].frontend_id);
  }
  ui::ViewAndroid* view_android = web_contents_->GetNativeView();
  if (!view_android)
    return;

  const ScopedJavaLocalRef<jobject> current_view = anchor_view_.view();
  if (current_view.is_null())
    anchor_view_ = view_android->AcquireAnchorView();

  const ScopedJavaLocalRef<jobject> view = anchor_view_.view();
  if (view.is_null())
    return;

  view_android->SetAnchorRect(view, element_bounds);
  Java_AwAutofillClient_showAutofillPopup(env, obj, view, is_rtl, data_array);
}

void AwAutofillClient::UpdateAutofillPopupDataListValues(
    const std::vector<base::string16>& values,
    const std::vector<base::string16>& labels) {
  // Leaving as an empty method since updating autofill popup window
  // dynamically does not seem to be a useful feature for android webview.
  // See crrev.com/18102002 if need to implement.
}

void AwAutofillClient::HideAutofillPopup() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  delegate_.reset();
  Java_AwAutofillClient_hideAutofillPopup(env, obj);
}

bool AwAutofillClient::IsAutocompleteEnabled() {
  bool enabled = GetSaveFormData();
  if (!autocomplete_uma_recorded_) {
    UMA_HISTOGRAM_BOOLEAN("Autofill.AutocompleteEnabled", enabled);
    autocomplete_uma_recorded_ = true;
  }
  return enabled;
}

void AwAutofillClient::PropagateAutofillPredictions(
    content::RenderFrameHost* rfh,
    const std::vector<autofill::FormStructure*>& forms) {}

void AwAutofillClient::DidFillOrPreviewField(
    const base::string16& autofilled_value,
    const base::string16& profile_full_name) {}

void AwAutofillClient::DidInteractWithNonsecureCreditCardInput() {}

bool AwAutofillClient::IsContextSecure() {
  content::SSLStatus ssl_status;
  content::NavigationEntry* navigation_entry =
      web_contents_->GetController().GetLastCommittedEntry();
  if (!navigation_entry)
    return false;

  ssl_status = navigation_entry->GetSSL();
  // Note: The implementation below is a copy of the one in
  // ChromeAutofillClient::IsContextSecure, and should be kept in sync
  // until crbug.com/505388 gets implemented.
  return navigation_entry->GetURL().SchemeIsCryptographic() &&
         ssl_status.certificate &&
         (!net::IsCertStatusError(ssl_status.cert_status) ||
          net::IsCertStatusMinorError(ssl_status.cert_status)) &&
         !(ssl_status.content_status &
           content::SSLStatus::RAN_INSECURE_CONTENT);
}

bool AwAutofillClient::ShouldShowSigninPromo() {
  return false;
}

void AwAutofillClient::ExecuteCommand(int id) {
  NOTIMPLEMENTED();
}

bool AwAutofillClient::AreServerCardsSupported() {
  return true;
}

void AwAutofillClient::Dismissed(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj) {
  anchor_view_.Reset();
}

void AwAutofillClient::SuggestionSelected(JNIEnv* env,
                                          const JavaParamRef<jobject>& object,
                                          jint position) {
  if (delegate_) {
    delegate_->DidAcceptSuggestion(suggestions_[position].value,
                                   suggestions_[position].frontend_id,
                                   position);
  }
}

void AwAutofillClient::ShowAutofillSettings(bool show_credit_card_settings) {
  NOTIMPLEMENTED();
}

void AwAutofillClient::ShowUnmaskPrompt(
    const autofill::CreditCard& card,
    UnmaskCardReason reason,
    base::WeakPtr<autofill::CardUnmaskDelegate> delegate) {
  NOTIMPLEMENTED();
}

void AwAutofillClient::OnUnmaskVerificationResult(PaymentsRpcResult result) {
  NOTIMPLEMENTED();
}

void AwAutofillClient::ShowLocalCardMigrationDialog(
    base::OnceClosure show_migration_dialog_closure) {
  NOTIMPLEMENTED();
}

void AwAutofillClient::ConfirmMigrateLocalCardToCloud(
    std::unique_ptr<base::DictionaryValue> legal_message,
    const std::vector<autofill::MigratableCreditCard>& migratable_credit_cards,
    LocalCardMigrationCallback start_migrating_cards_callback) {
  NOTIMPLEMENTED();
}

void AwAutofillClient::ConfirmSaveAutofillProfile(
    const autofill::AutofillProfile& profile,
    base::OnceClosure callback) {
  // Since there is no confirmation needed to save an Autofill Profile,
  // running |callback| will proceed with saving |profile|.
  std::move(callback).Run();
}

void AwAutofillClient::ConfirmSaveCreditCardLocally(
    const autofill::CreditCard& card,
    bool show_prompt,
    base::OnceClosure callback) {
  NOTIMPLEMENTED();
}

void AwAutofillClient::ConfirmSaveCreditCardToCloud(
    const autofill::CreditCard& card,
    std::unique_ptr<base::DictionaryValue> legal_message,
    bool should_request_name_from_user,
    bool show_prompt,
    base::OnceCallback<void(const base::string16&)> callback) {
  NOTIMPLEMENTED();
}

void AwAutofillClient::ConfirmCreditCardFillAssist(
    const autofill::CreditCard& card,
    const base::Closure& callback) {
  NOTIMPLEMENTED();
}

void AwAutofillClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  NOTIMPLEMENTED();
}

bool AwAutofillClient::HasCreditCardScanFeature() {
  return false;
}

void AwAutofillClient::ScanCreditCard(const CreditCardScanCallback& callback) {
  NOTIMPLEMENTED();
}

}  // namespace android_webview
