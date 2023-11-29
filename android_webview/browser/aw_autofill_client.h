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
#include "components/autofill/core/browser/autofill_trigger_details.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/android/view_android.h"

namespace autofill {
class AutocompleteHistoryManager;
class AutofillPopupDelegate;
class CreditCard;
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

// Manager delegate for the autofill functionality.
//
// Android O and beyond uses `AndroidAutofillManager`, unlike Chrome, which
// uses `BrowserAutofillManager`.
//
// Android WebView supports enabling Autofill feature for each webview
// instance (different than the browser which supports enabling/disabling for a
// profile). Since there is only one pref service for a given browser context,
// we cannot enable this feature via UserPrefs. Rather, we always keep the
// feature enabled at the pref service, and control it via the delegates.
// Lifetime: WebView
class AwAutofillClient : public autofill::ContentAutofillClient {
 public:
  static void CreateForWebContents(content::WebContents* contents);

  AwAutofillClient(const AwAutofillClient&) = delete;
  AwAutofillClient& operator=(const AwAutofillClient&) = delete;

  ~AwAutofillClient() override;

  // AutofillClient:
  bool IsOffTheRecord() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  autofill::AutofillCrowdsourcingManager* GetCrowdsourcingManager() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  autofill::AutocompleteHistoryManager* GetAutocompleteHistoryManager()
      override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  syncer::SyncService* GetSyncService() override;
  signin::IdentityManager* GetIdentityManager() override;
  autofill::FormDataImporter* GetFormDataImporter() override;
  autofill::payments::PaymentsNetworkInterface* GetPaymentsNetworkInterface()
      override;
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
  void ConfirmCreditCardFillAssist(const autofill::CreditCard& card,
                                   base::OnceClosure callback) override;
  void ConfirmSaveAddressProfile(
      const autofill::AutofillProfile& profile,
      const autofill::AutofillProfile* original_profile,
      SaveAddressProfilePromptOptions options,
      AddressProfileSavePromptCallback callback) override;
  void ShowEditAddressProfileDialog(
      const autofill::AutofillProfile& profile,
      AddressProfileSavePromptCallback on_user_decision_callback) override;
  void ShowDeleteAddressProfileDialog(
      const autofill::AutofillProfile& profile,
      AddressProfileDeleteDialogCallback delete_dialog_callback) override;
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
      base::span<const autofill::SelectOption> datalist) override;
  std::vector<autofill::Suggestion> GetPopupSuggestions() const override;
  void PinPopupView() override;
  autofill::AutofillClient::PopupOpenArgs GetReopenPopupArgs(
      autofill::AutofillSuggestionTriggerSource trigger_source) const override;
  void UpdatePopup(
      const std::vector<autofill::Suggestion>& suggestions,
      autofill::PopupType popup_type,
      autofill::AutofillSuggestionTriggerSource trigger_source) override;
  void HideAutofillPopup(autofill::PopupHidingReason reason) override;
  bool IsAutocompleteEnabled() const override;
  bool IsPasswordManagerEnabled() override;
  void DidFillOrPreviewForm(
      autofill::mojom::ActionPersistence action_persistence,
      autofill::AutofillTriggerSource trigger_source,
      bool is_refill) override;
  void DidFillOrPreviewField(const std::u16string& autofilled_value,
                             const std::u16string& profile_full_name) override;
  bool IsContextSecure() const override;
  autofill::FormInteractionsFlowId GetCurrentFormInteractionsFlowId() override;

  // RiskDataLoader:
  void LoadRiskData(
      base::OnceCallback<void(const std::string&)> callback) override;

  void Dismissed(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void SuggestionSelected(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          jint position);

 private:
  friend class content::WebContentsUserData<AwAutofillClient>;

  explicit AwAutofillClient(content::WebContents* web_contents);

  void ShowAutofillPopupImpl(
      const gfx::RectF& element_bounds,
      bool is_rtl,
      const std::vector<autofill::Suggestion>& suggestions);

  content::WebContents& GetWebContents() const;

  JavaObjectWeakGlobalRef java_ref_;

  ui::ViewAndroid::ScopedAnchorView anchor_view_;

  // The current Autofill query values.
  std::vector<autofill::Suggestion> suggestions_;
  base::WeakPtr<autofill::AutofillPopupDelegate> delegate_;
  std::unique_ptr<autofill::AutofillCrowdsourcingManager>
      crowdsourcing_manager_;

#if DCHECK_IS_ON()
  bool use_android_autofill_manager_;
#endif
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_AUTOFILL_CLIENT_H_
