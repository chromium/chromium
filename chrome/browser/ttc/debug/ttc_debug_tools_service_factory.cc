// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/debug/ttc_debug_tools_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/debug/ttc_debug_tools_service.h"

namespace ttc {

// static
TtcDebugToolsService* TtcDebugToolsServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<TtcDebugToolsService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
TtcDebugToolsServiceFactory* TtcDebugToolsServiceFactory::GetInstance() {
  static base::NoDestructor<TtcDebugToolsServiceFactory> instance;
  return instance.get();
}

TtcDebugToolsServiceFactory::TtcDebugToolsServiceFactory()
    : ProfileKeyedServiceFactory("TtcDebugToolsService",
                                 ProfileSelections::BuildForRegularProfile()) {}

TtcDebugToolsServiceFactory::~TtcDebugToolsServiceFactory() = default;

std::unique_ptr<KeyedService>
TtcDebugToolsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(kTtc) ||
      !base::FeatureList::IsEnabled(kTtcDebugTools)) {
    return nullptr;
  }
  return std::make_unique<TtcDebugToolsService>();
}

}  // namespace ttc
