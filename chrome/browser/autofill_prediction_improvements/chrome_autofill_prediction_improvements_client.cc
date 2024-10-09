// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_prediction_improvements/chrome_autofill_prediction_improvements_client.h"

#include "base/check_deref.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/strike_database_factory.h"
#include "chrome/browser/feedback/public/feedback_source.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/user_annotations/user_annotations_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_client.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_features.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_filling_engine_impl.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_manager.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_annotations/user_annotations_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/base/l10n/l10n_util.h"

ChromeAutofillPredictionImprovementsClient::
    ChromeAutofillPredictionImprovementsClient(
        content::WebContents* web_contents)
    : content::WebContentsUserData<ChromeAutofillPredictionImprovementsClient>(
          *web_contents),
      prefs_(CHECK_DEREF(
          Profile::FromBrowserContext(GetWebContents().GetBrowserContext())
              ->GetPrefs())),
      prediction_improvements_manager_{
          this,
          OptimizationGuideKeyedServiceFactory::GetForProfile(
              Profile::FromBrowserContext(
                  GetWebContents().GetBrowserContext())),
          autofill::StrikeDatabaseFactory::GetForProfile(
              Profile::FromBrowserContext(
                  GetWebContents().GetBrowserContext())),
      } {}

ChromeAutofillPredictionImprovementsClient::
    ~ChromeAutofillPredictionImprovementsClient() = default;

// static
std::unique_ptr<ChromeAutofillPredictionImprovementsClient>
ChromeAutofillPredictionImprovementsClient::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  if (!autofill_prediction_improvements::
          IsAutofillPredictionImprovementsEnabled()) {
    return nullptr;
  }
  return base::WrapUnique<ChromeAutofillPredictionImprovementsClient>(
      new ChromeAutofillPredictionImprovementsClient(web_contents));
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
  GetWebContents().RequestAXTreeSnapshot(
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
        Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
    filling_engine_ =
        std::make_unique<autofill_prediction_improvements::
                             AutofillPredictionImprovementsFillingEngineImpl>(
            OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
            UserAnnotationsServiceFactory::GetForProfile(profile));
  }
  return filling_engine_.get();
}

const GURL& ChromeAutofillPredictionImprovementsClient::GetLastCommittedURL() {
  return GetWebContents().GetPrimaryMainFrame()->GetLastCommittedURL();
}

std::string ChromeAutofillPredictionImprovementsClient::GetTitle() {
  return base::UTF16ToUTF8(GetWebContents().GetTitle());
}

user_annotations::UserAnnotationsService*
ChromeAutofillPredictionImprovementsClient::GetUserAnnotationsService() {
  Profile* profile =
      Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
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
          Profile::FromBrowserContext(GetWebContents().GetBrowserContext()));
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
      GetWebContents().GetLastCommittedURL(),
      Profile::FromBrowserContext(GetWebContents().GetBrowserContext()),
      feedback::kFeedbackSourceAI,
      /*description_template=*/std::string(),
      /*description_placeholder_text=*/
      l10n_util::GetStringUTF8(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FEEDBACK_PLACEHOLDER),
      /*category_tag=*/"autofill_prediction_improvements",
      /*extra_diagnostics=*/std::string(),
      /*autofill_metadata=*/base::Value::Dict(), std::move(feedback_metadata));
}

void ChromeAutofillPredictionImprovementsClient::
    OpenPredictionImprovementsSettings() {
  GetWebContents().OpenURL(
      content::OpenURLParams(
          GURL(chrome::kAutofillPredictionImprovementsSubPage),
          content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui::PAGE_TRANSITION_LINK,
          /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});
}

bool ChromeAutofillPredictionImprovementsClient::IsUserEligible() {
  Profile* profile =
      Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return false;
  }
  signin_util::SignedInState state =
      signin_util::GetSignedInState(identity_manager);
  return state == signin_util::SignedInState::kSignedIn ||
         state == signin_util::SignedInState::kSyncing;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeAutofillPredictionImprovementsClient);
