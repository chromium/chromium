// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_AUTOFILL_CLIENT_H_
#define ANDROID_WEBVIEW_BROWSER_AW_AUTOFILL_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/android/view_android.h"

namespace autofill {
class AutocompleteHistoryManager;
class AutofillDriver;
class AutofillPopupDelegate;
class CreditCard;
class FormStructure;
class PersonalDataManager;
class StrikeDatabase;
struct CardUnmaskPromptOptions;
}  // namespace autofill

namespace content {
class WebContents;
}

namespace gfx {
class RectF;
}

namespace syncer {
class SyncService;
}

class PersonalDataManager;
class PrefService;

namespace android_webview {

// Manager delegate for the autofill functionality.
//
// Android O and beyond shall use `AndroidAutofillManager` (and
// `AutofillProvider`), whereas earlier versions use a `BrowserAutofillManager`.
// This is determined by the `use_android_autofill_manager` parameters below.
//
// Android webview supports enabling autocomplete feature for each webview
// instance (different than the browser which supports enabling/disabling for a
// profile). Since there is only one pref service for a given browser context,
// we cannot enable this feature via UserPrefs. Rather, we always keep the
// feature enabled at the pref service, and control it via the delegates.
class AwAutofillClient : public autofill::ContentAutofillClient {
 public:
  static AwAutofillClient* FromWebContents(content::WebContents* web_contents) {
    return static_cast<AwAutofillClient*>(
        ContentAutofillClient::FromWebContents(web_contents));
  }

  // The `use_android_autofill_manager` parameter determines which
  // DriverInitCallback to use:
  // - autofill::BrowserDriverInitHook() (to be used before Android O) or
  // - android_webview::AndroidDriverInitHook() (to be used as of Android O).
  static void CreateForWebContents(content::WebContents* contents,
                                   bool use_android_autofill_manager);

  AwAutofillClient(const AwAutofillClient&) = delete;
  AwAutofillClient& operator=(const AwAutofillClient&) = delete;

  ~AwAutofillClient() override;

  void SetSaveFormData(bool enabled);
  bool GetSaveFormData() const;

  // AutofillClient:
  bool IsOffTheRecord() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  autofill::AutofillDownloadManager* GetDownloadManager() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  autofill::AutocompleteHistoryManager* GetAutocompleteHistoryManager()
      override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  syncer::SyncService* GetSyncService() override;
  signin::IdentityManager* GetIdentityManager() override;
  autofill::FormDataImporter* GetFormDataImporter() override;
  autofill::payments::PaymentsClient* GetPaymentsClient() override;
  autofill::StrikeDatabase* GetStrikeDatabase() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  ukm::SourceId GetUkmSourceId() override;
  autofill::AddressNormalizer* GetAddressNormalizer() override;
  const GURL& GetLastCommittedPrimaryMainFrameURL() const override;
  url::Origin GetLastCommittedPrimaryMainFrameOrigin() const override;
  security_state::SecurityLevel GetSecurityLevelForUmaHistograms() override;
  const translate::LanguageState* GetLanguageState() override;
  translate::TranslateDriver* GetTranslateDriver() override;
  void ShowAutofillSettings(autofill::PopupType popup_type) override;
  void ShowUnmaskPrompt(
      const autofill::CreditCard& card,
      const autofill::CardUnmaskPromptOptions& card_unmask_prompt_options,
      base::WeakPtr<autofill::CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(PaymentsRpcResult result) override;
  void ConfirmAccountNameFixFlow(
      base::OnceCallback<void(const std::u16string&)> callback) override;
  void ConfirmExpirationDateFixFlow(
      const autofill::CreditCard& card,
      base::OnceCallback<void(const std::u16string&, const std::u16string&)>
          callback) override;
  void ConfirmSaveCreditCardLocally(
      const autofill::CreditCard& card,
      SaveCreditCardOptions options,
      LocalSaveCardPromptCallback callback) override;
  void ConfirmSaveCreditCardToCloud(
      const autofill::CreditCard& card,
      const autofill::LegalMessageLines& legal_message_lines,
      SaveCreditCardOptions options,
      UploadSaveCardPromptCallback callback) override;
  void CreditCardUploadCompleted(bool card_saved) override;
  void ConfirmCreditCardFillAssist(const autofill::CreditCard& card,
                                   base::OnceClosure callback) override;
  void ConfirmSaveAddressProfile(
      const autofill::AutofillProfile& profile,
      const autofill::AutofillProfile* original_profile,
      SaveAddressProfilePromptOptions options,
      AddressProfileSavePromptCallback callback) override;
  bool HasCreditCardScanFeature() override;
  void ScanCreditCard(CreditCardScanCallback callback) override;
  bool IsTouchToFillCreditCardSupported() override;
  bool ShowTouchToFillCreditCard(
      base::WeakPtr<autofill::TouchToFillDelegate> delegate,
      base::span<const autofill::CreditCard> cards_to_suggest) override;
  void HideTouchToFillCreditCard() override;
  void ShowAutofillPopup(
      const autofill::AutofillClient::PopupOpenArgs& open_args,
      base::WeakPtr<autofill::AutofillPopupDelegate> delegate) override;
  void UpdateAutofillPopupDataListValues(
      const std::vector<std::u16string>& values,
      const std::vector<std::u16string>& labels) override;
  std::vector<autofill::Suggestion> GetPopupSuggestions() const override;
  void PinPopupView() override;
  autofill::AutofillClient::PopupOpenArgs GetReopenPopupArgs() const override;
  void UpdatePopup(const std::vector<autofill::Suggestion>& suggestions,
                   autofill::PopupType popup_type) override;
  void HideAutofillPopup(autofill::PopupHidingReason reason) override;
  bool IsAutocompleteEnabled() const override;
  bool IsPasswordManagerEnabled() override;
  void PropagateAutofillPredictions(
      autofill::AutofillDriver* driver,
      const std::vector<autofill::FormStructure*>& forms) override;
  void DidFillOrPreviewForm(autofill::mojom::RendererFormDataAction action,
                            autofill::AutofillTriggerSource trigger_source,
                            bool is_refill) override;
  void DidFillOrPreviewField(const std::u16string& autofilled_value,
                             const std::u16string& profile_full_name) override;
  bool IsContextSecure() const override;
  void ExecuteCommand(int id) override;
  void OpenPromoCodeOfferDetailsURL(const GURL& url) override;
  autofill::FormInteractionsFlowId GetCurrentFormInteractionsFlowId() override;

  // RiskDataLoader:
  void LoadRiskData(
      base::OnceCallback<void(const std::string&)> callback) override;

  void Dismissed(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void SuggestionSelected(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          jint position);

 private:
  // `use_android_autofill_manager` determines which DriverInitCallback to use
  // for the ContentAutofillDriverFactory: autofill::BrowserDriverInitHook() or
  // android_webview::AndroidDriverInitHook().
  AwAutofillClient(content::WebContents* web_contents,
                   bool use_android_autofill_manager);
  friend class content::WebContentsUserData<AwAutofillClient>;

  void ShowAutofillPopupImpl(
      const gfx::RectF& element_bounds,
      bool is_rtl,
      const std::vector<autofill::Suggestion>& suggestions);

  content::WebContents& GetWebContents() const;

  bool save_form_data_ = false;
  JavaObjectWeakGlobalRef java_ref_;

  ui::ViewAndroid::ScopedAnchorView anchor_view_;

  // The current Autofill query values.
  std::vector<autofill::Suggestion> suggestions_;
  base::WeakPtr<autofill::AutofillPopupDelegate> delegate_;
  std::unique_ptr<autofill::AutofillDownloadManager> download_manager_;

#if DCHECK_IS_ON()
  bool use_android_autofill_manager_;
#endif
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_AUTOFILL_CLIENT_H_
