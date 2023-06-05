// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scalable_iph/scalable_iph_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace ash {

namespace {
constexpr char kScalableIphServiceName[] = "ScalableIphKeyedService";
}

ScalableIphFactory::ScalableIphFactory()
    : ProfileKeyedServiceFactory(kScalableIphServiceName) {
  DependsOn(feature_engagement::TrackerFactory::GetInstance());
}

ScalableIphFactory::~ScalableIphFactory() = default;

ScalableIphFactory* ScalableIphFactory::GetInstance() {
  static base::NoDestructor<ScalableIphFactory> instance;
  return instance.get();
}

scalable_iph::ScalableIph* ScalableIphFactory::GetForProfile(Profile* profile) {
  return static_cast<scalable_iph::ScalableIph*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

std::unique_ptr<KeyedService>
ScalableIphFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(browser_context);
  CHECK(tracker);

  return std::make_unique<scalable_iph::ScalableIph>(tracker);
}

}  // namespace ash
