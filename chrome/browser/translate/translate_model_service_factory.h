// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_MODEL_SERVICE_FACTORY_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_MODEL_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"
#include "components/translate/content/browser/translate_model_service.h"

class SimpleFactoryKey;

// LazyInstance that owns all TranslateModelService(s) and associates
// them with Profiles.
class TranslateModelServiceFactory : public SimpleKeyedServiceFactory {
 public:
  // Gets the TranslateModelService for the profile.
  //
  // Returns null if the features that allow for this to provide useful
  // information are disabled. Importantly, only available when the
  // optimization guide service is.
  static translate::TranslateModelService* GetOrBuildForKey(
      SimpleFactoryKey* key);

  // Gets the LazyInstance that owns all TranslateModelService(s).
  static TranslateModelServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<TranslateModelServiceFactory>;

  TranslateModelServiceFactory();
  ~TranslateModelServiceFactory() override;

  // SimpleKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;
  SimpleFactoryKey* GetKeyToUse(SimpleFactoryKey* key) const override;
};

#endif  //  CHROME_BROWSER_TRANSLATE_TRANSLATE_MODEL_SERVICE_FACTORY_H_
