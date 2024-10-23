// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_prediction_improvements/chrome_autofill_prediction_improvements_client.h"

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/strike_database_factory.h"
#include "chrome/browser/autofill_prediction_improvements/autofill_prediction_improvements_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feedback/public/feedback_source.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/user_annotations/user_annotations_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/field_filling_address_util.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_client.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_features.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_filling_engine_impl.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_manager.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/signin/public/base/consent_level.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_annotations/user_annotations_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/base/l10n/l10n_util.h"

ChromeAutofillPredictionImprovementsClient::
    ChromeAutofillPredictionImprovementsClient(
        content::WebContents* web_contents,
        Profile* profile)
    : web_contents_(CHECK_DEREF(web_contents)),
      prefs_(CHECK_DEREF(profile->GetPrefs())),
      prediction_improvements_manager_{
          this,
          OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
          autofill::StrikeDatabaseFactory::GetForProfile(profile),
      } {}

ChromeAutofillPredictionImprovementsClient::
    ~ChromeAutofillPredictionImprovementsClient() = default;

// static
std::unique_ptr<ChromeAutofillPredictionImprovementsClient>
ChromeAutofillPredictionImprovementsClient::MaybeCreateForWebContents(
    content::WebContents* web_contents,
    Profile* profile) {
  if (!autofill_prediction_improvements::
          IsAutofillPredictionImprovementsSupported(profile->GetPrefs())) {
    return nullptr;
  }
  return base::WrapUnique<ChromeAutofillPredictionImprovementsClient>(
      new ChromeAutofillPredictionImprovementsClient(web_contents, profile));
}

void ChromeAutofillPredictionImprovementsClient::GetAXTree(
    AXTreeCallback callback) {
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

autofill_prediction_improvements::AutofillPredictionImprovementsManager&
ChromeAutofillPredictionImprovementsClient::GetManager() {
  return prediction_improvements_manager_;
}

autofill_prediction_improvements::AutofillPredictionImprovementsFillingEngine*
ChromeAutofillPredictionImprovementsClient::GetFillingEngine() {
  if (!filling_engine_) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents_->GetBrowserContext());
    filling_engine_ =
        std::make_unique<autofill_prediction_improvements::
                             AutofillPredictionImprovementsFillingEngineImpl>(
            OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
            UserAnnotationsServiceFactory::GetForProfile(profile));
  }
  return filling_engine_.get();
}

const GURL& ChromeAutofillPredictionImprovementsClient::GetLastCommittedURL() {
  return web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL();
}

std::string ChromeAutofillPredictionImprovementsClient::GetTitle() {
  return base::UTF16ToUTF8(web_contents_->GetTitle());
}

user_annotations::UserAnnotationsService*
ChromeAutofillPredictionImprovementsClient::GetUserAnnotationsService() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  return profile ? UserAnnotationsServiceFactory::GetForProfile(profile)
                 : nullptr;
}

bool ChromeAutofillPredictionImprovementsClient::
    IsAutofillPredictionImprovementsEnabledPref() const {
  return prefs_->GetBoolean(
      autofill::prefs::kAutofillPredictionImprovementsEnabled);
}

bool ChromeAutofillPredictionImprovementsClient::CanShowFeedbackPage() {
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

void ChromeAutofillPredictionImprovementsClient::TryToOpenFeedbackPage(
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

void ChromeAutofillPredictionImprovementsClient::
    OpenPredictionImprovementsSettings() {
  web_contents_->OpenURL(
      content::OpenURLParams(
          GURL(chrome::kAutofillPredictionImprovementsSubPage),
          content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui::PAGE_TRANSITION_LINK,
          /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});
}

bool ChromeAutofillPredictionImprovementsClient::IsUserEligible() {
  return autofill_prediction_improvements::IsUserEligible(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
}

autofill::FormStructure*
ChromeAutofillPredictionImprovementsClient::GetCachedFormStructure(
    const autofill::FormData& form_data) {
  autofill::ContentAutofillDriver* driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(
          web_contents_->GetPrimaryMainFrame());
  if (!driver) {
    return nullptr;
  }
  return driver->GetAutofillManager().FindCachedFormById(form_data.global_id());
}

std::u16string
ChromeAutofillPredictionImprovementsClient::GetAutofillFillingValue(
    const std::string& autofill_profile_guid,
    autofill::FieldType field_type,
    const autofill::FormFieldData& field) {
  autofill::ContentAutofillDriverFactory* driver_factory =
      autofill::ContentAutofillDriverFactory::FromWebContents(
          &web_contents_.get());
  if (!driver_factory) {
    return u"";
  }
  autofill::PersonalDataManager* pdm =
      driver_factory->client().GetPersonalDataManager();
  if (!pdm) {
    return u"";
  }
  const autofill::AutofillProfile* autofill_profile =
      pdm->address_data_manager().GetProfileByGUID(autofill_profile_guid);
  if (!autofill_profile) {
    return u"";
  }
  std::vector<std::pair<autofill::FieldGlobalId, std::u16string>>
      autofill_filling_values;

  const autofill::AutofillType autofill_type =
      autofill::AutofillType(field_type);
  if (autofill::FieldTypeGroupToFormType(autofill_type.group()) !=
      autofill::FormType::kAddressForm) {
    return u"";
  }
  const auto& [filling_value, filling_type] = GetFillingValueAndTypeForProfile(
      *autofill_profile, g_browser_process->GetApplicationLocale(),
      autofill::AutofillType(autofill_type), field,
      driver_factory->client().GetAddressNormalizer());

  return filling_value;
}
