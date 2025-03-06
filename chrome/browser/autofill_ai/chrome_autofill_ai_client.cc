// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_ai/chrome_autofill_ai_client.h"

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/strike_database_factory.h"
#include "chrome/browser/autofill_ai/autofill_ai_util.h"
#include "chrome/browser/browser_process.h"
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
#include "components/autofill/core/browser/filling/addresses/field_filling_address_util.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_executor_impl.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill_ai/core/browser/autofill_ai_client.h"
#include "components/autofill_ai/core/browser/autofill_ai_features.h"
#include "components/autofill_ai/core/browser/autofill_ai_manager.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/signin/public/base/consent_level.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

namespace {

using autofill::AttributeInstance;
using autofill::AttributeType;
using autofill::AttributeTypeName;

}  // namespace

ChromeAutofillAiClient::ChromeAutofillAiClient(
    content::WebContents* web_contents,
    Profile* profile)
    : web_contents_(CHECK_DEREF(web_contents)),
      prefs_(CHECK_DEREF(profile->GetPrefs())),
      prediction_improvements_manager_{
          this,
          autofill::StrikeDatabaseFactory::GetForProfile(profile),
      } {
  DCHECK(
      autofill_ai::AutofillAiIsPlatformAndEnterprisePolicyEligible(&*prefs_));
}

ChromeAutofillAiClient::~ChromeAutofillAiClient() = default;

// static
std::unique_ptr<ChromeAutofillAiClient>
ChromeAutofillAiClient::MaybeCreateForWebContents(
    content::WebContents* web_contents,
    Profile* profile) {
  if (!autofill_ai::AutofillAiIsPlatformAndEnterprisePolicyEligible(
          profile->GetPrefs())) {
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

autofill::EntityDataManager* ChromeAutofillAiClient::GetEntityDataManager() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  return profile ? autofill::AutofillEntityDataManagerFactory::GetForProfile(
                       profile)
                 : nullptr;
}

bool ChromeAutofillAiClient::IsAutofillAiEnabledPref() const {
  return prefs_->GetBoolean(
      autofill::prefs::kAutofillPredictionImprovementsEnabled);
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

bool ChromeAutofillAiClient::IsUserEligible() {
  return autofill_ai::IsUserEligible(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
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
