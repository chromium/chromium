// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_AUTOFILL_CLIENT_H_
#define ANDROID_WEBVIEW_BROWSER_AW_AUTOFILL_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/android/view_android.h"

namespace autofill {
class AutocompleteHistoryManager;
class AutofillPopupDelegate;
class CardUnmaskDelegate;
class CreditCard;
class FormStructure;
class PersonalDataManager;
class StrikeDatabase;
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

// Manager delegate for the autofill functionality. Android webview
// supports enabling autocomplete feature for each webview instance
// (different than the browser which supports enabling/disabling for
// a profile). Since there is only one pref service for a given browser
// context, we cannot enable this feature via UserPrefs. Rather, we always
// keep the feature enabled at the pref service, and control it via
// the delegates.
class AwAutofillClient : public autofill::AutofillClient,
                         public content::WebContentsUserData<AwAutofillClient> {
 public:
  ~AwAutofillClient() override;

  void SetSaveFormData(bool enabled);
  bool GetSaveFormData();

  // AutofillClient:
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  autofill::AutocompleteHistoryManager* GetAutocompleteHistoryManager()
      override;
  PrefService* GetPrefs() override;
  syncer::SyncService* GetSyncService() override;
  signin::IdentityManager* GetIdentityManager() override;
  autofill::FormDataImporter* GetFormDataImporter() override;
  autofill::payments::PaymentsClient* GetPaymentsClient() override;
  autofill::StrikeDatabase* GetStrikeDatabase() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  ukm::SourceId GetUkmSourceId() override;
  autofill::AddressNormalizer* GetAddressNormalizer() override;
  const GURL& GetLastCommittedURL() override;
  security_state::SecurityLevel GetSecurityLevelForUmaHistograms() override;
  const translate::LanguageState* GetLanguageState() override;
  void ShowAutofillSettings(bool show_credit_card_settings) override;
  void ShowUnmaskPrompt(
      const autofill::CreditCard& card,
      UnmaskCardReason reason,
      base::WeakPtr<autofill::CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(PaymentsRpcResult result) override;
  void ConfirmAccountNameFixFlow(
      base::OnceCallback<void(const base::string16&)> callback) override;
  void ConfirmExpirationDateFixFlow(
      const autofill::CreditCard& card,
      base::OnceCallback<void(const base::string16&, const base::string16&)>
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
  bool HasCreditCardScanFeature() override;
  void ScanCreditCard(CreditCardScanCallback callback) override;
  void ShowAutofillPopup(
      const autofill::AutofillClient::PopupOpenArgs& open_args,
      base::WeakPtr<autofill::AutofillPopupDelegate> delegate) override;
  void UpdateAutofillPopupDataListValues(
      const std::vector<base::string16>& values,
      const std::vector<base::string16>& labels) override;
  base::span<const autofill::Suggestion> GetPopupSuggestions() const override;
  void PinPopupView() override;
  autofill::AutofillClient::PopupOpenArgs GetReopenPopupArgs() const override;
  void UpdatePopup(const std::vector<autofill::Suggestion>& suggestions,
                   autofill::PopupType popup_type) override;
  void HideAutofillPopup(autofill::PopupHidingReason reason) override;
  bool IsAutocompleteEnabled() override;
  void PropagateAutofillPredictions(
      content::RenderFrameHost* rfh,
      const std::vector<autofill::FormStructure*>& forms) override;
  void DidFillOrPreviewField(const base::string16& autofilled_value,
                             const base::string16& profile_full_name) override;
  bool IsContextSecure() override;
  bool ShouldShowSigninPromo() override;
  bool AreServerCardsSupported() override;
  void ExecuteCommand(int id) override;

  // RiskDataLoader:
  void LoadRiskData(
      base::OnceCallback<void(const std::string&)> callback) override;

  void Dismissed(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void SuggestionSelected(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          jint position);

 private:
  explicit AwAutofillClient(content::WebContents* web_contents);
  friend class content::WebContentsUserData<AwAutofillClient>;

  void ShowAutofillPopupImpl(
      const gfx::RectF& element_bounds,
      bool is_rtl,
      const std::vector<autofill::Suggestion>& suggestions);

  // The web_contents associated with this delegate.
  content::WebContents* web_contents_;
  bool save_form_data_ = false;
  JavaObjectWeakGlobalRef java_ref_;

  ui::ViewAndroid::ScopedAnchorView anchor_view_;

  // The current Autofill query values.
  std::vector<autofill::Suggestion> suggestions_;
  base::WeakPtr<autofill::AutofillPopupDelegate> delegate_;

  // Tracks whether the autocomplete enabled metric has already been logged for
  // this client.
  bool autocomplete_uma_recorded_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(AwAutofillClient);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_AUTOFILL_CLIENT_H_
