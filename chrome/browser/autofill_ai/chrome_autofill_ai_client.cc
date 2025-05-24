// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_ai/chrome_autofill_ai_client.h"

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/autofill/strike_database_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/autofill/autofill_ai/save_or_update_autofill_ai_data_controller.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill_ai/core/browser/autofill_ai_client.h"
#include "components/autofill_ai/core/browser/autofill_ai_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

ChromeAutofillAiClient::ChromeAutofillAiClient(
    content::WebContents* web_contents)
    : web_contents_(CHECK_DEREF(web_contents)),
      prediction_improvements_manager_{
          this,
          autofill::StrikeDatabaseFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      } {
  DCHECK(base::FeatureList::IsEnabled(
      autofill::features::kAutofillAiWithDataSchema));
}

ChromeAutofillAiClient::~ChromeAutofillAiClient() = default;

// static
std::unique_ptr<ChromeAutofillAiClient>
ChromeAutofillAiClient::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillAiWithDataSchema)) {
    return nullptr;
  }
  return base::WrapUnique<ChromeAutofillAiClient>(
      new ChromeAutofillAiClient(web_contents));
}

autofill::ContentAutofillClient& ChromeAutofillAiClient::GetAutofillClient() {
  // TODO: crbug.com/371534239 - Make the lifecycle relationships explicit.
  return CHECK_DEREF(
      autofill::ContentAutofillClient::FromWebContents(&*web_contents_));
}

autofill_ai::AutofillAiManager& ChromeAutofillAiClient::GetManager() {
  return prediction_improvements_manager_;
}

autofill::EntityDataManager* ChromeAutofillAiClient::GetEntityDataManager() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  return profile ? autofill::AutofillEntityDataManagerFactory::GetForProfile(
                       profile)
                 : nullptr;
}

void ChromeAutofillAiClient::ShowSaveOrUpdateBubble(
    autofill::EntityInstance new_entity,
    std::optional<autofill::EntityInstance> old_entity,
    SaveOrUpdatePromptResultCallback prompt_acceptance_callback) {
#if !BUILDFLAG(IS_ANDROID)
  if (auto* controller =
          autofill_ai::SaveOrUpdateAutofillAiDataController::GetOrCreate(
              &*web_contents_, GetAutofillClient().GetAppLocale())) {
    controller->ShowPrompt(std::move(new_entity), std::move(old_entity),
                           std::move(prompt_acceptance_callback));
    return;
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  std::move(prompt_acceptance_callback).Run(SaveOrUpdatePromptResult());
}

autofill::FormStructure* ChromeAutofillAiClient::GetCachedFormStructure(
    const autofill::FormGlobalId& form_id) {
  autofill::ContentAutofillDriver* driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(
          web_contents_->GetPrimaryMainFrame());
  if (!driver) {
    return nullptr;
  }
  return driver->GetAutofillManager().FindCachedFormById(form_id);
}

optimization_guide::ModelQualityLogsUploaderService*
ChromeAutofillAiClient::GetMqlsUploadService() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  OptimizationGuideKeyedService* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!optimization_guide_keyed_service) {
    return nullptr;
  }
  return optimization_guide_keyed_service->GetModelQualityLogsUploaderService();
}
