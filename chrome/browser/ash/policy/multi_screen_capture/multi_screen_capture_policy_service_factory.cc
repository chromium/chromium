// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/multi_screen_capture/multi_screen_capture_policy_service_factory.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/policy/multi_screen_capture/multi_screen_capture_policy_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#error This file should only be included on Ash ChromeOS.
#endif

namespace policy {

MultiScreenCapturePolicyService*
MultiScreenCapturePolicyServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<MultiScreenCapturePolicyService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

MultiScreenCapturePolicyServiceFactory*
MultiScreenCapturePolicyServiceFactory::GetInstance() {
  static base::NoDestructor<MultiScreenCapturePolicyServiceFactory> instance;
  return instance.get();
}

MultiScreenCapturePolicyServiceFactory::MultiScreenCapturePolicyServiceFactory()
    : ProfileKeyedServiceFactory(
          "MultiScreenCapturePolicyServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

MultiScreenCapturePolicyServiceFactory::
    ~MultiScreenCapturePolicyServiceFactory() = default;

std::unique_ptr<KeyedService>
MultiScreenCapturePolicyServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return MultiScreenCapturePolicyService::Create(
      Profile::FromBrowserContext(context));
}

bool MultiScreenCapturePolicyServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace policy
