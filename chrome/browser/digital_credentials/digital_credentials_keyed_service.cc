// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/digital_credentials/digital_credentials_keyed_service.h"

#include "base/feature_list.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "url/gurl.h"

namespace digital_credentials {

namespace {
BASE_FEATURE(kEnableDigitalCredentialsCreationWithBrowserContext,
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace

DigitalCredentialsKeyedService::DigitalCredentialsKeyedService(
    OptimizationGuideKeyedService& optimization_guide_service)
    : optimization_guide_service_(optimization_guide_service) {
  optimization_guide_service_->RegisterOptimizationTypes(
      {optimization_guide::proto::OptimizationType::
           DIGITAL_CREDENTIALS_LOW_FRICTION});
}

DigitalCredentialsKeyedService::~DigitalCredentialsKeyedService() = default;

bool DigitalCredentialsKeyedService::IsLowFrictionUrl(
    const GURL& to_check) const {
  switch (optimization_guide_service_->CanApplyOptimization(
      to_check,
      optimization_guide::proto::OptimizationType::
          DIGITAL_CREDENTIALS_LOW_FRICTION,
      nullptr)) {
    case optimization_guide::OptimizationGuideDecision::kTrue:
      return true;
    case optimization_guide::OptimizationGuideDecision::kFalse:
      return false;
    case optimization_guide::OptimizationGuideDecision::kUnknown:
      return false;
  }
}

// --- Factory ---

DigitalCredentialsKeyedService*
DigitalCredentialsKeyedServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<DigitalCredentialsKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

DigitalCredentialsKeyedServiceFactory*
DigitalCredentialsKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<DigitalCredentialsKeyedServiceFactory> instance;
  return instance.get();
}

DigitalCredentialsKeyedServiceFactory::DigitalCredentialsKeyedServiceFactory()
    : ProfileKeyedServiceFactory(
          "DigitalCredentialsKeyedService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

DigitalCredentialsKeyedServiceFactory::
    ~DigitalCredentialsKeyedServiceFactory() = default;

std::unique_ptr<KeyedService>
DigitalCredentialsKeyedServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  OptimizationGuideKeyedService* optimization_guide_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  if (!optimization_guide_service) {
    return nullptr;
  }
  return std::make_unique<DigitalCredentialsKeyedService>(
      *optimization_guide_service);
}

bool DigitalCredentialsKeyedServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return base::FeatureList::IsEnabled(
      kEnableDigitalCredentialsCreationWithBrowserContext);
}

}  // namespace digital_credentials
