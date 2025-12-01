// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_field_classification_model_handler_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/autofill/ml_log_router_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_change_service.h"
#include "chrome/browser/password_manager/password_change_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/ml_model/logging/ml_log_router.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "content/public/browser/browser_context.h"

// static
PasswordFieldClassificationModelHandlerFactory*
PasswordFieldClassificationModelHandlerFactory::GetInstance() {
  static base::NoDestructor<PasswordFieldClassificationModelHandlerFactory>
      instance;
  return instance.get();
}

// static
autofill::FieldClassificationModelHandler*
PasswordFieldClassificationModelHandlerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<autofill::FieldClassificationModelHandler*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

PasswordFieldClassificationModelHandlerFactory::
    PasswordFieldClassificationModelHandlerFactory()
    : BrowserContextKeyedServiceFactory(
          "FieldClassificationModelHandler",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

PasswordFieldClassificationModelHandlerFactory::
    ~PasswordFieldClassificationModelHandlerFactory() = default;

bool PasswordFieldClassificationModelHandlerFactory::
    ServiceIsCreatedWithBrowserContext() const {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return true;
#endif  // BUILDFLAG(IS_ANDROID)
}

content::BrowserContext*
PasswordFieldClassificationModelHandlerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  // `FieldClassificationModelHandler` is not supported without an
  // `OptimizationGuideKeyedService`.
  if (!OptimizationGuideKeyedServiceFactory::GetForProfile(profile)) {
    return nullptr;
  }

  // Main feature is enabled, no need to check anything else.
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordFormClientsideClassifier)) {
    return context;
  }

  // Special case for Automated Password Change which uses a model in a very
  // limited scope.
  ChromePasswordChangeService* password_change_service =
      PasswordChangeServiceFactory::GetForProfile(profile);
  if (password_change_service &&
      password_change_service->UserIsActivePasswordChangeUser()) {
    return context;
  }

#if !BUILDFLAG(IS_ANDROID)
  // Special case for ActorLogin which uses a model in a very limited scope.
  if (profile->GetPrefs()->HasPrefPath(
          glic::prefs::kGlicUserEnabledActuationOnWeb) &&
      profile->GetPrefs()->GetBoolean(
          glic::prefs::kGlicUserEnabledActuationOnWeb) &&
      base::FeatureList::IsEnabled(
          password_manager::features::kActorLoginLocalClassificationModel)) {
    return context;
  }
#endif

  return nullptr;
}

std::unique_ptr<KeyedService> PasswordFieldClassificationModelHandlerFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  OptimizationGuideKeyedService* optimization_guide =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  autofill::MlLogRouter* log_router =
      autofill::MlLogRouterFactory::GetForProfile(profile);
  return std::make_unique<autofill::FieldClassificationModelHandler>(
      optimization_guide,
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_PASSWORD_MANAGER_FORM_CLASSIFICATION,
      log_router);
}
