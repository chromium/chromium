// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_FIELD_CLASSIFICATION_MODEL_HANDLER_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_FIELD_CLASSIFICATION_MODEL_HANDLER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/autofill/core/browser/ml_model/field_classification_model_handler.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

// A factory for creating one `FieldClassificationModelHandler` per browser
// context.
class PasswordFieldClassificationModelHandlerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static PasswordFieldClassificationModelHandlerFactory* GetInstance();
  static autofill::FieldClassificationModelHandler* GetForBrowserContext(
      content::BrowserContext* context);

  PasswordFieldClassificationModelHandlerFactory(
      const PasswordFieldClassificationModelHandlerFactory&) = delete;
  PasswordFieldClassificationModelHandlerFactory& operator=(
      const PasswordFieldClassificationModelHandlerFactory&) = delete;

 private:
  friend base::NoDestructor<PasswordFieldClassificationModelHandlerFactory>;

  PasswordFieldClassificationModelHandlerFactory();
  ~PasswordFieldClassificationModelHandlerFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_FIELD_CLASSIFICATION_MODEL_HANDLER_FACTORY_H_
