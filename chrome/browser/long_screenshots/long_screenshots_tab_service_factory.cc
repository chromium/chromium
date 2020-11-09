// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/long_screenshots/long_screenshots_tab_service_factory.h"

#include "build/build_config.h"
#include "chrome/browser/long_screenshots/long_screenshots_tab_service.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_factory_key.h"

namespace long_screenshots {

namespace {

constexpr char kFeatureDirname[] = "long_screenshots_tab_service";

}  // namespace

// static
LongScreenshotsTabServiceFactory*
LongScreenshotsTabServiceFactory::GetInstance() {
  return base::Singleton<LongScreenshotsTabServiceFactory>::get();
}

// static
long_screenshots::LongScreenshotsTabService*
LongScreenshotsTabServiceFactory::GetServiceInstance(SimpleFactoryKey* key) {
  return static_cast<long_screenshots::LongScreenshotsTabService*>(
      GetInstance()->GetServiceForKey(key, true));
}

LongScreenshotsTabServiceFactory::LongScreenshotsTabServiceFactory()
    : SimpleKeyedServiceFactory("LongScreenshotsTabService",
                                SimpleDependencyManager::GetInstance()) {}

LongScreenshotsTabServiceFactory::~LongScreenshotsTabServiceFactory() = default;

std::unique_ptr<KeyedService>
LongScreenshotsTabServiceFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  // Prevent this working off the record.
  if (key->IsOffTheRecord())
    return nullptr;

  return std::make_unique<LongScreenshotsTabService>(
      key->GetPath(), kFeatureDirname, nullptr, key->IsOffTheRecord());
}

SimpleFactoryKey* LongScreenshotsTabServiceFactory::GetKeyToUse(
    SimpleFactoryKey* key) const {
  return key;
}

}  // namespace long_screenshots
