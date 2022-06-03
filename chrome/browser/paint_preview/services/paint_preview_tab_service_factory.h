// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAINT_PREVIEW_SERVICES_PAINT_PREVIEW_TAB_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PAINT_PREVIEW_SERVICES_PAINT_PREVIEW_TAB_SERVICE_FACTORY_H_

#include <memory>

#include "base/memory/singleton.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"

class SimpleFactoryKey;

namespace paint_preview {
class PaintPreviewTabService;

// Factory to create one PaintPreviewTabService per profile key.
class PaintPreviewTabServiceFactory : public SimpleKeyedServiceFactory {
 public:
  static PaintPreviewTabServiceFactory* GetInstance();

  static paint_preview::PaintPreviewTabService* GetServiceInstance(
      SimpleFactoryKey* key);

  PaintPreviewTabServiceFactory(const PaintPreviewTabServiceFactory&) = delete;
  PaintPreviewTabServiceFactory& operator=(
      const PaintPreviewTabServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<PaintPreviewTabServiceFactory>;

  PaintPreviewTabServiceFactory();
  ~PaintPreviewTabServiceFactory() override;

  // SimpleKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;
  SimpleFactoryKey* GetKeyToUse(SimpleFactoryKey* key) const override;
};

}  // namespace paint_preview

#endif  // CHROME_BROWSER_PAINT_PREVIEW_SERVICES_PAINT_PREVIEW_TAB_SERVICE_FACTORY_H_
