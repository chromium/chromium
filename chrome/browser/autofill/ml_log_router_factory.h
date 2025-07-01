// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_AUTOFILL_ML_LOG_ROUTER_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_ML_LOG_ROUTER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/autofill/core/browser/ml_model/logging/ml_log_router.h"

namespace autofill {
class MLLogRouter;

// A factory for creating one `MLLogRouter` per browser context.
class MLLogRouterFactory : public ProfileKeyedServiceFactory {
 public:
  static MLLogRouterFactory* GetInstance();
  static autofill::MLLogRouter* GetForProfile(Profile* profile);

  MLLogRouterFactory(const MLLogRouterFactory&) = delete;
  MLLogRouterFactory& operator=(const MLLogRouterFactory&) = delete;

 private:
  friend base::NoDestructor<MLLogRouterFactory>;

  MLLogRouterFactory();
  ~MLLogRouterFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ML_LOG_ROUTER_FACTORY_H_
