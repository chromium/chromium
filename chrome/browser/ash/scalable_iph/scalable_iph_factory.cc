// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scalable_iph/scalable_iph_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/scalable_iph/scalable_iph_delegate_impl.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace ash {

namespace {
constexpr char kScalableIphServiceName[] = "ScalableIphKeyedService";
}

ScalableIphFactory::ScalableIphFactory()
    : BrowserContextKeyedServiceFactory(
          kScalableIphServiceName,
          BrowserContextDependencyManager::GetInstance()) {
  CHECK(delegate_testing_factory_.is_null())
      << "Testing factory must be null at initialization.";

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

void ScalableIphFactory::SetDelegateFactoryForTesting(
    DelegateTestingFactory delegate_testing_factory) {
  CHECK(delegate_testing_factory_.is_null())
      << "It's NOT allowed to set DelegateTestingFactory twice";

  delegate_testing_factory_ = std::move(delegate_testing_factory);
}

content::BrowserContext* ScalableIphFactory::GetBrowserContextToUse(
    content::BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile) {
    return nullptr;
  }

  if (!profile->IsRegularProfile() || profile->IsChild()) {
    return nullptr;
  }

  return browser_context;
}

std::unique_ptr<KeyedService>
ScalableIphFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(browser_context);
  CHECK(tracker) << "No tracker. This method cannot handle this error. "
                    "BuildServiceInstanceForBrowserContext method is not "
                    "allowed to return nullptr";

  Profile* profile = Profile::FromBrowserContext(browser_context);
  CHECK(profile) << "No profile. This method cannot handle this error. "
                    "BuildServiceInstanceForBrowserContext method is not "
                    "allowed to return nullptr";

  return std::make_unique<scalable_iph::ScalableIph>(
      tracker, CreateScalableIphDelegate(profile));
}

std::unique_ptr<scalable_iph::ScalableIphDelegate>
ScalableIphFactory::CreateScalableIphDelegate(Profile* profile) const {
  CHECK(profile) << "Profile must not be nullptr for this method";

  if (!delegate_testing_factory_.is_null()) {
    return delegate_testing_factory_.Run();
  }

  return std::make_unique<ScalableIphDelegateImpl>(profile);
}

}  // namespace ash
