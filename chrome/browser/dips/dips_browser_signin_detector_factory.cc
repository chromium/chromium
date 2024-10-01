// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_browser_signin_detector_factory.h"

#include "chrome/browser/dips/chrome_dips_delegate.h"
#include "chrome/browser/dips/dips_browser_signin_detector.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/common/content_features.h"

/*static*/
DIPSBrowserSigninDetector* DIPSBrowserSigninDetector::Get(
    content::BrowserContext* browser_context) {
  return DIPSBrowserSigninDetectorFactory::GetForBrowserContext(
      browser_context);
}

/*static*/
DIPSBrowserSigninDetectorFactory*
DIPSBrowserSigninDetectorFactory::GetInstance() {
  static base::NoDestructor<DIPSBrowserSigninDetectorFactory> instance(
      PassKey{});
  return instance.get();
}

/*static*/
DIPSBrowserSigninDetector*
DIPSBrowserSigninDetectorFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DIPSBrowserSigninDetector*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

DIPSBrowserSigninDetectorFactory::DIPSBrowserSigninDetectorFactory(PassKey)
    : BrowserContextKeyedServiceFactory(
          "DIPSBrowserSigninDetector",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(DIPSServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

DIPSBrowserSigninDetectorFactory::~DIPSBrowserSigninDetectorFactory() = default;

content::BrowserContext*
DIPSBrowserSigninDetectorFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kDIPS)) {
    return nullptr;
  }

  if (!ChromeDipsDelegate::Create()->ShouldEnableDips(context)) {
    return nullptr;
  }

  return context;
}

void DIPSBrowserSigninDetectorFactory::EnableWaitForServiceForTesting() {
  context_runloops_for_testing_.emplace();
}

std::unique_ptr<KeyedService>
DIPSBrowserSigninDetectorFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (context_runloops_for_testing_.has_value()) {
    // Unblock any calls to `WaitForServiceForTesting()` with `context`.
    context_runloops_for_testing_.value()[context->UniqueId()].Quit();
  }

  return std::make_unique<DIPSBrowserSigninDetector>(
      base::PassKey<DIPSBrowserSigninDetectorFactory>(),
      DIPSService::Get(context),
      IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(context)));
}

void DIPSBrowserSigninDetectorFactory::WaitForServiceForTesting(
    content::BrowserContext* browser_context) {
  context_runloops_for_testing_.value()[browser_context->UniqueId()].Run();
  CHECK(GetServiceForBrowserContext(browser_context, /*create=*/false));
}
