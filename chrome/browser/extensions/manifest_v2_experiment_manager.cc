// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"

#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"

namespace extensions {

namespace {

class ManifestV2ExperimentManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  ManifestV2ExperimentManagerFactory();
  ManifestV2ExperimentManagerFactory(
      const ManifestV2ExperimentManagerFactory&) = delete;
  ManifestV2ExperimentManagerFactory& operator=(
      const ManifestV2ExperimentManagerFactory&) = delete;
  ~ManifestV2ExperimentManagerFactory() override = default;

  ManifestV2ExperimentManager* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

ManifestV2ExperimentManagerFactory::ManifestV2ExperimentManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "ManifestV2ExperimentManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionManagementFactory::GetInstance());
}

ManifestV2ExperimentManager*
ManifestV2ExperimentManagerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<ManifestV2ExperimentManager*>(
      GetServiceForBrowserContext(browser_context, /*create=*/true));
}

content::BrowserContext*
ManifestV2ExperimentManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Shared instance between incognito and regular profiles. This matches the
  // rest of the core extension services, such as ExtensionService.
  return ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
      context, /*force_guest_profile=*/true);
}

KeyedService* ManifestV2ExperimentManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ManifestV2ExperimentManager(context);
}

MV2ExperimentStage CalculateCurrentExperimentStage() {
  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionManifestV2DeprecationWarning)) {
    return MV2ExperimentStage::kWarning;
  }

  return MV2ExperimentStage::kNone;
}

}  // namespace

ManifestV2ExperimentManager::ManifestV2ExperimentManager(
    content::BrowserContext* browser_context)
    : experiment_stage_(CalculateCurrentExperimentStage()),
      // Note: passing `ExtensionManagement` is safe and guaranteed to outlive
      // the `impact_checker_` because this class is a KeyedService that depends
      // on `ExtensionManagement`.
      impact_checker_(
          experiment_stage_,
          ExtensionManagementFactory::GetForBrowserContext(browser_context)) {}
ManifestV2ExperimentManager::~ManifestV2ExperimentManager() = default;

// static
ManifestV2ExperimentManager* ManifestV2ExperimentManager::Get(
    content::BrowserContext* browser_context) {
  return static_cast<ManifestV2ExperimentManagerFactory*>(GetFactory())
      ->GetForBrowserContext(browser_context);
}

// static
BrowserContextKeyedServiceFactory* ManifestV2ExperimentManager::GetFactory() {
  static base::NoDestructor<ManifestV2ExperimentManagerFactory> g_factory;
  return g_factory.get();
}

MV2ExperimentStage ManifestV2ExperimentManager::GetCurrentExperimentStage() {
  return experiment_stage_;
}

bool ManifestV2ExperimentManager::IsExtensionAffected(
    const Extension& extension) {
  return impact_checker_.IsExtensionAffected(extension);
}

}  // namespace extensions
