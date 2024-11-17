// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_ai/chrome_autofill_ai_client.h"

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/strike_database_factory.h"
#include "chrome/browser/autofill_ai/autofill_ai_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feedback/public/feedback_source.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/autofill/autofill_prediction_improvements/save_autofill_prediction_improvements_controller.h"
#include "chrome/browser/user_annotations/user_annotations_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/field_filling_address_util.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill_ai/core/browser/autofill_ai_client.h"
#include "components/autofill_ai/core/browser/autofill_ai_features.h"
#include "components/autofill_ai/core/browser/autofill_ai_manager.h"
#include "components/autofill_ai/core/browser/suggestion/autofill_ai_model_executor_impl.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/signin/public/base/consent_level.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_annotations/user_annotations_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

ChromeAutofillAiClient::ChromeAutofillAiClient(
    content::WebContents* web_contents,
    Profile* profile)
    : web_contents_(CHECK_DEREF(web_contents)),
      prefs_(CHECK_DEREF(profile->GetPrefs())),
      prediction_improvements_manager_{
          this,
          OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
          autofill::StrikeDatabaseFactory::GetForProfile(profile),
      } {
  DCHECK(autofill_ai::IsAutofillAiSupported(&*prefs_));
}

ChromeAutofillAiClient::~ChromeAutofillAiClient() = default;

// static
std::unique_ptr<ChromeAutofillAiClient>
ChromeAutofillAiClient::MaybeCreateForWebContents(
    content::WebContents* web_contents,
    Profile* profile) {
  if (!autofill_ai::IsAutofillAiSupported(profile->GetPrefs())) {
    return nullptr;
  }
  return base::WrapUnique<ChromeAutofillAiClient>(
      new ChromeAutofillAiClient(web_contents, profile));
}

autofill::ContentAutofillClient& ChromeAutofillAiClient::GetAutofillClient() {
  // TODO: crbug.com/371534239 - Make the lifecycle relationships explicit.
  return CHECK_DEREF(
      autofill::ContentAutofillClient::FromWebContents(&*web_contents_));
}

void ChromeAutofillAiClient::GetAXTree(AXTreeCallback callback) {
  using ProtoTreeUpdate = optimization_guide::proto::AXTreeUpdate;
  base::OnceCallback<ProtoTreeUpdate(ui::AXTreeUpdate&)> processing_callback =
      base::BindOnce([](ui::AXTreeUpdate& ax_tree_update) {
        ProtoTreeUpdate ax_tree_proto;
        optimization_guide::PopulateAXTreeUpdateProto(ax_tree_update,
                                                      &ax_tree_proto);
        return ax_tree_proto;
      });
  web_contents_->RequestAXTreeSnapshot(
      std::move(processing_callback).Then(std::move(callback)),
      ui::kAXModeWebContentsOnly,
      /*max_nodes=*/500,
      /*timeout=*/{},
      content::WebContents::AXTreeSnapshotPolicy::kSameOriginDirectDescendants);
}

autofill_ai::AutofillAiManager& ChromeAutofillAiClient::GetManager() {
  return prediction_improvements_manager_;
}

autofill_ai::AutofillAiModelExecutor*
ChromeAutofillAiClient::GetModelExecutor() {
  if (!filling_engine_) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents_->GetBrowserContext());
    filling_engine_ =
        std::make_unique<autofill_ai::AutofillAiModelExecutorImpl>(
            OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
            UserAnnotationsServiceFactory::GetForProfile(profile));
  }
  return filling_engine_.get();
}

const GURL& ChromeAutofillAiClient::GetLastCommittedURL() {
  return web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL();
}

const url::Origin& ChromeAutofillAiClient::GetLastCommittedOrigin() {
  return web_contents_->GetPrimaryMainFrame()->GetLastCommittedOrigin();
}

std::string ChromeAutofillAiClient::GetTitle() {
  return base::UTF16ToUTF8(web_contents_->GetTitle());
}

user_annotations::UserAnnotationsService*
ChromeAutofillAiClient::GetUserAnnotationsService() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  return profile ? UserAnnotationsServiceFactory::GetForProfile(profile)
                 : nullptr;
}

bool ChromeAutofillAiClient::IsAutofillAiEnabledPref() const {
  return prefs_->GetBoolean(
      autofill::prefs::kAutofillPredictionImprovementsEnabled);
}

bool ChromeAutofillAiClient::CanShowFeedbackPage() {
  OptimizationGuideKeyedService* opt_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
  if (!opt_guide_keyed_service ||
      !opt_guide_keyed_service->ShouldFeatureBeCurrentlyAllowedForFeedback(
          optimization_guide::proto::LogAiDataRequest::FeatureCase::
              kFormsPredictions)) {
    return false;
  }

  return true;
}

void ChromeAutofillAiClient::TryToOpenFeedbackPage(
    const std::string& feedback_id) {
  if (!CanShowFeedbackPage()) {
    return;
  }
  base::Value::Dict feedback_metadata;
  feedback_metadata.Set("log_id", feedback_id);

  chrome::ShowFeedbackPage(
      web_contents_->GetLastCommittedURL(),
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
      feedback::kFeedbackSourceAI,
      /*description_template=*/std::string(),
      /*description_placeholder_text=*/
      l10n_util::GetStringUTF8(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FEEDBACK_PLACEHOLDER),
      /*category_tag=*/"autofill_with_ai",
      /*extra_diagnostics=*/std::string(),
      /*autofill_metadata=*/base::Value::Dict(), std::move(feedback_metadata));
}

void ChromeAutofillAiClient::OpenPredictionImprovementsSettings() {
  web_contents_->OpenURL(
      content::OpenURLParams(
          GURL(base::StrCat({"chrome://settings/",
                             chrome::kAutofillPredictionImprovementsSubPage})),
          content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui::PAGE_TRANSITION_LINK,
          /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});
}

void ChromeAutofillAiClient::ShowSaveAutofillAiBubble(
    std::unique_ptr<user_annotations::FormAnnotationResponse>
        form_annotation_response,
    user_annotations::PromptAcceptanceCallback prompt_acceptance_callback) {
#if !BUILDFLAG(IS_ANDROID)
  if (auto* controller =
          autofill::SaveAutofillPredictionImprovementsController::GetOrCreate(
              &*web_contents_)) {
    controller->OfferSave(
        std::move(form_annotation_response->to_be_upserted_entries),
        std::move(prompt_acceptance_callback),
        base::BindRepeating(
            &autofill_ai::AutofillAiManager::UserClickedLearnMore,
            prediction_improvements_manager_.GetWeakPtr()),
        base::BindRepeating(&autofill_ai::AutofillAiManager::
                                SaveAutofillPredictionsUserFeedbackReceived,
                            prediction_improvements_manager_.GetWeakPtr(),
                            form_annotation_response->model_execution_id));
    return;
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  std::move(prompt_acceptance_callback).Run({/*prompt_was_accepted=*/false});
}

bool ChromeAutofillAiClient::IsUserEligible() {
  return autofill_ai::IsUserEligible(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
}

autofill::FormStructure* ChromeAutofillAiClient::GetCachedFormStructure(
    const autofill::FormData& form_data) {
  autofill::ContentAutofillDriver* driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(
          web_contents_->GetPrimaryMainFrame());
  if (!driver) {
    return nullptr;
  }
  return driver->GetAutofillManager().FindCachedFormById(form_data.global_id());
}

std::u16string ChromeAutofillAiClient::GetAutofillNameFillingValue(
    const std::string& autofill_profile_guid,
    autofill::FieldType field_type,
    const autofill::FormFieldData& field) {
  autofill::PersonalDataManager* pdm =
      autofill::PersonalDataManagerFactory::GetForBrowserContext(
          web_contents_->GetBrowserContext());
  if (!pdm) {
    return u"";
  }
  const autofill::AutofillProfile* autofill_profile =
      pdm->address_data_manager().GetProfileByGUID(autofill_profile_guid);
  if (!autofill_profile) {
    return u"";
  }
  if (autofill::GroupTypeOfFieldType(field_type) !=
      autofill::FieldTypeGroup::kName) {
    return u"";
  }
  // Note that since we are only interested in name values, the address
  // normalizer is not needed.
  const auto& [filling_value, filling_type] = GetFillingValueAndTypeForProfile(
      *autofill_profile, g_browser_process->GetApplicationLocale(),
      autofill::AutofillType(field_type), field,
      /*address_normalizer=*/nullptr);

  return filling_value;
}
