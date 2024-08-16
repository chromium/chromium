// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_prediction_improvements/chrome_autofill_prediction_improvements_client.h"

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/user_annotations/user_annotations_service_factory.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_client.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_filling_engine_impl.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_manager.h"
#include "components/compose/buildflags.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/accessibility/ax_tree_update.h"

// TODO(crbug.com/359116403): Move AX serialization out of `compose`.
#if BUILDFLAG(ENABLE_COMPOSE)
#include "chrome/browser/compose/compose_ax_serialization_utils.h"
#endif

ChromeAutofillPredictionImprovementsClient::
    ChromeAutofillPredictionImprovementsClient(
        content::WebContents* web_contents)
    : content::WebContentsUserData<ChromeAutofillPredictionImprovementsClient>(
          *web_contents) {}

ChromeAutofillPredictionImprovementsClient::
    ~ChromeAutofillPredictionImprovementsClient() = default;

// static
std::unique_ptr<ChromeAutofillPredictionImprovementsClient>
ChromeAutofillPredictionImprovementsClient::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillPredictionImprovementsEnabled)) {
    return nullptr;
  }
  return base::WrapUnique<ChromeAutofillPredictionImprovementsClient>(
      new ChromeAutofillPredictionImprovementsClient(web_contents));
}

void ChromeAutofillPredictionImprovementsClient::GetAXTree(
    AXTreeCallback callback) {
  using ProtoTreeUpdate = optimization_guide::proto::AXTreeUpdate;
  base::OnceCallback<ProtoTreeUpdate(const ui::AXTreeUpdate&)>
      processing_callback =
          base::BindOnce([](const ui::AXTreeUpdate& ax_tree_update) {
            ProtoTreeUpdate ax_tree_proto;
#if BUILDFLAG(ENABLE_COMPOSE)
            ComposeAXSerializationUtils::PopulateAXTreeUpdate(ax_tree_update,
                                                              &ax_tree_proto);
#endif
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
  return manager_;
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

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeAutofillPredictionImprovementsClient);
