// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_internals_ui.h"

#include <memory>
#include <vector>

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/grit/optimization_guide_internals_resources.h"
#include "components/grit/optimization_guide_internals_resources_map.h"
#include "components/optimization_guide/core/prediction_manager.h"
#include "components/optimization_guide/optimization_guide_internals/webui/optimization_guide_internals.mojom.h"
#include "components/optimization_guide/optimization_guide_internals/webui/optimization_guide_internals_page_handler_impl.h"

// static
OptimizationGuideInternalsUI*
OptimizationGuideInternalsUI::MaybeCreateOptimizationGuideInternalsUI(
    content::WebUI* web_ui,
    SetupWebUIDataSourceCallback set_up_data_source_callback) {
  Profile* profile = Profile::FromWebUI(web_ui);
  auto* service = OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!service)
    return nullptr;
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
  // TODO(https://crbug.com/1297362): Remove the reset which is needed now since
  // |this| is reused on internals page reloads.
  optimization_guide_internals_page_factory_receiver_.reset();
  optimization_guide_internals_page_factory_receiver_.Bind(std::move(receiver));
}

void OptimizationGuideInternalsUI::CreatePageHandler(
    mojo::PendingRemote<optimization_guide_internals::mojom::Page> page) {
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* service = OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  DCHECK(service);
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
  DCHECK(service);
  optimization_guide::PredictionManager* prediction_manager =
      service->GetPredictionManager();
  std::vector<optimization_guide_internals::mojom::DownloadedModelInfoPtr>
      downloaded_models_info =
          prediction_manager->GetDownloadedModelsInfoForWebUI();
  std::move(callback).Run(std::move(downloaded_models_info));
}

WEB_UI_CONTROLLER_TYPE_IMPL(OptimizationGuideInternalsUI)
