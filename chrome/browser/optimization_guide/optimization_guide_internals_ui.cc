// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/optimization_guide/optimization_guide_internals_ui.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "base/hash/hash.h"
#include "base/i18n/time_formatting.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/grit/optimization_guide_internals_resources.h"
#include "components/grit/optimization_guide_internals_resources_map.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_quality/model_quality_util.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/prediction_manager.h"
#include "components/optimization_guide/optimization_guide_internals/webui/optimization_guide_internals.mojom.h"
#include "components/optimization_guide/optimization_guide_internals/webui/optimization_guide_internals_page_handler_impl.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/prefs/pref_service.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

// static
OptimizationGuideInternalsUI*
OptimizationGuideInternalsUI::MaybeCreateOptimizationGuideInternalsUI(
    content::WebUI* web_ui,
    SetupWebUIDataSourceCallback set_up_data_source_callback) {
  return new OptimizationGuideInternalsUI(
      web_ui, std::move(set_up_data_source_callback));
}

OptimizationGuideInternalsUI::OptimizationGuideInternalsUI(
    content::WebUI* web_ui,
    SetupWebUIDataSourceCallback set_up_data_source_callback)
    : MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  std::move(set_up_data_source_callback)
      .Run(base::make_span(kOptimizationGuideInternalsResources,
                           kOptimizationGuideInternalsResourcesSize),
           IDR_OPTIMIZATION_GUIDE_INTERNALS_OPTIMIZATION_GUIDE_INTERNALS_HTML);
}

OptimizationGuideInternalsUI::~OptimizationGuideInternalsUI() = default;

void OptimizationGuideInternalsUI::BindInterface(
    mojo::PendingReceiver<
        optimization_guide_internals::mojom::PageHandlerFactory> receiver) {
  // TODO(crbug.com/40215132): Remove the reset which is needed now since
  // |this| is reused on internals page reloads.
  optimization_guide_internals_page_factory_receiver_.reset();
  optimization_guide_internals_page_factory_receiver_.Bind(std::move(receiver));
}

void OptimizationGuideInternalsUI::CreatePageHandler(
    mojo::PendingRemote<optimization_guide_internals::mojom::Page> page) {
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* service = OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!service) {
    return;
  }
  OptimizationGuideLogger* optimization_guide_logger =
      service->GetOptimizationGuideLogger();
  optimization_guide_internals_page_handler_ =
      std::make_unique<OptimizationGuideInternalsPageHandlerImpl>(
          std::move(page), optimization_guide_logger);
}

void OptimizationGuideInternalsUI::RequestDownloadedModelsInfo(
    RequestDownloadedModelsInfoCallback callback) {
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* service = OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!service) {
    return;
  }
  optimization_guide::PredictionManager* prediction_manager =
      service->GetPredictionManager();
  std::vector<optimization_guide_internals::mojom::DownloadedModelInfoPtr>
      downloaded_models_info =
          prediction_manager->GetDownloadedModelsInfoForWebUI();
  std::move(callback).Run(std::move(downloaded_models_info));
}

void OptimizationGuideInternalsUI::RequestLoggedModelQualityClientIds(
    RequestLoggedModelQualityClientIdsCallback callback) {
  PrefService* local_state = g_browser_process->local_state();

  // Get the client ids for the compose and tab organization feature for the
  // past 28 days to show on chrome://optimization-guide-internals.
  // TODO(b/308642692): Add other features client id as requested.
  std::vector<optimization_guide_internals::mojom::LoggedClientIdsPtr>
      logged_client_ids;

  int64_t client_id =
      local_state->GetInt64(optimization_guide::model_execution::prefs::
                                localstate::kModelQualityLogggingClientId);

  // If the client id is zero no client id is set, in that case do nothing.
  if (client_id == 0) {
    return;
  }

  // Initialize time outside to have it change when generating the client ids
  // for different days.
  base::Time now = base::Time::Now();
  // Loop through past 28 days to generate the client ids for compose and
  // tab_organization features.
  for (int i = 0; i < 28; ++i) {
    base::Time day_i = now - base::Days(i);

    // Hash the client id with the date so that it changes everyday for every
    // feature.
    int64_t client_id_i_compose =
        optimization_guide::GetHashedModelQualityClientId(
            optimization_guide::proto::LogAiDataRequest::FeatureCase::kCompose,
            day_i, client_id);

    int64_t client_id_i_tab_organization =
        optimization_guide::GetHashedModelQualityClientId(
            optimization_guide::proto::LogAiDataRequest::FeatureCase::
                kTabOrganization,
            day_i, client_id);

    logged_client_ids.push_back(
        optimization_guide_internals::mojom::LoggedClientIds::New(
            client_id_i_compose));
    logged_client_ids.push_back(
        optimization_guide_internals::mojom::LoggedClientIds::New(
            client_id_i_tab_organization));
  }

  std::move(callback).Run(std::move(logged_client_ids));
}

WEB_UI_CONTROLLER_TYPE_IMPL(OptimizationGuideInternalsUI)
