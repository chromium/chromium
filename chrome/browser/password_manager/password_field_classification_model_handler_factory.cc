// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_field_classification_model_handler_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
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

content::BrowserContext*
PasswordFieldClassificationModelHandlerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // `FieldClassificationModelHandler` is not supported without an
  // `OptimizationGuideKeyedService`.
  return OptimizationGuideKeyedServiceFactory::GetForProfile(
             Profile::FromBrowserContext(context))
             ? context
             : nullptr;
}

std::unique_ptr<KeyedService> PasswordFieldClassificationModelHandlerFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  OptimizationGuideKeyedService* optimization_guide =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  return std::make_unique<autofill::FieldClassificationModelHandler>(
      optimization_guide,
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_PASSWORD_MANAGER_FORM_CLASSIFICATION);
}
