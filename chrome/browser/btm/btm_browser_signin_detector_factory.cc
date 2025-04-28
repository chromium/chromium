// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/btm/btm_browser_signin_detector_factory.h"

#include "chrome/browser/btm/btm_browser_signin_detector.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/btm_service.h"
#include "content/public/common/content_features.h"

/*static*/
BtmBrowserSigninDetector* BtmBrowserSigninDetector::Get(
    content::BrowserContext* browser_context) {
  return BtmBrowserSigninDetectorFactory::GetForBrowserContext(browser_context);
}

/*static*/
BtmBrowserSigninDetectorFactory*
BtmBrowserSigninDetectorFactory::GetInstance() {
  static base::NoDestructor<BtmBrowserSigninDetectorFactory> instance(
      PassKey{});
  return instance.get();
}

/*static*/
BtmBrowserSigninDetector* BtmBrowserSigninDetectorFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<BtmBrowserSigninDetector*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

BtmBrowserSigninDetectorFactory::BtmBrowserSigninDetectorFactory(PassKey)
    : BrowserContextKeyedServiceFactory(
          "BtmBrowserSigninDetector",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

BtmBrowserSigninDetectorFactory::~BtmBrowserSigninDetectorFactory() = default;

content::BrowserContext*
BtmBrowserSigninDetectorFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kBtm)) {
    return nullptr;
  }

  if (!ShouldBrowserContextEnableBtm(context)) {
    return nullptr;
  }

  return context;
}

void BtmBrowserSigninDetectorFactory::EnableWaitForServiceForTesting() {
  context_runloops_for_testing_.emplace();
}

std::unique_ptr<KeyedService>
BtmBrowserSigninDetectorFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (context_runloops_for_testing_.has_value()) {
    // Unblock any calls to `WaitForServiceForTesting()` with `context`.
    context_runloops_for_testing_.value()[context->UniqueId()].Quit();
  }

  return std::make_unique<BtmBrowserSigninDetector>(
      base::PassKey<BtmBrowserSigninDetectorFactory>(),
      content::BtmService::Get(context),
      IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(context)));
}

void BtmBrowserSigninDetectorFactory::WaitForServiceForTesting(
    content::BrowserContext* browser_context) {
  context_runloops_for_testing_.value()[browser_context->UniqueId()].Run();
  CHECK(GetServiceForBrowserContext(browser_context, /*create=*/false));
}
