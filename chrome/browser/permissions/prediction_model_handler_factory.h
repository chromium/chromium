// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PREDICTION_MODEL_HANDLER_FACTORY_H_
#define CHROME_BROWSER_PERMISSIONS_PREDICTION_MODEL_HANDLER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}
namespace permissions {
class PredictionModelHandler;
}

class PredictionModelHandlerFactory : public BrowserContextKeyedServiceFactory {
 public:
  PredictionModelHandlerFactory(const PredictionModelHandlerFactory&) = delete;
  PredictionModelHandlerFactory& operator=(
      const PredictionModelHandlerFactory&) = delete;

  static PredictionModelHandlerFactory* GetInstance();
  static permissions::PredictionModelHandler* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  PredictionModelHandlerFactory();
  ~PredictionModelHandlerFactory() override;
  friend struct base::DefaultSingletonTraits<PredictionModelHandlerFactory>;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PERMISSIONS_PREDICTION_MODEL_HANDLER_FACTORY_H_
