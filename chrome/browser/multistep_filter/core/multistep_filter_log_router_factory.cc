// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multistep_filter/core/multistep_filter_log_router_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/logging/multistep_filter_log_router_impl.h"

namespace multistep_filter {

// static
MultistepFilterLogRouterImpl* MultistepFilterLogRouterFactory::GetForProfile(
    Profile* profile) {
  return static_cast<MultistepFilterLogRouterImpl*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
MultistepFilterLogRouterFactory*
MultistepFilterLogRouterFactory::GetInstance() {
  static base::NoDestructor<MultistepFilterLogRouterFactory> instance;
  return instance.get();
}

MultistepFilterLogRouterFactory::MultistepFilterLogRouterFactory()
    : ProfileKeyedServiceFactory("MultistepFilterLogRouter",
                                 ProfileSelections::BuildForRegularProfile()) {}

MultistepFilterLogRouterFactory::~MultistepFilterLogRouterFactory() = default;

std::unique_ptr<KeyedService>
MultistepFilterLogRouterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(kMultistepFilter)) {
    return nullptr;
  }
  return std::make_unique<MultistepFilterLogRouterImpl>();
}

}  // namespace multistep_filter
