// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/scalable_iph/scalable_iph_factory.h"

#include "ash/constants/ash_features.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/printing/synced_printers_manager_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/scalable_iph/scalable_iph_delegate_impl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace {

constexpr char kScalableIphServiceName[] = "ScalableIphKeyedService";

const user_manager::User* GetUser(content::BrowserContext* browser_context) {
  return ash::BrowserContextHelper::Get()->GetUserByBrowserContext(
      browser_context);
}

bool IsSupportedEmailDomain(content::BrowserContext* browser_context) {
  const std::string email =
      GetUser(browser_context)->GetAccountId().GetUserEmail();
  DCHECK(!email.empty());

  return gaia::IsGoogleInternalAccountEmail(email);
}

}  // namespace

ScalableIphFactory::ScalableIphFactory()
    : BrowserContextKeyedServiceFactory(
          kScalableIphServiceName,
          BrowserContextDependencyManager::GetInstance()) {
  CHECK(delegate_testing_factory_.is_null())
      << "Testing factory must be null at initialization.";

  DependsOn(feature_engagement::TrackerFactory::GetInstance());
  DependsOn(ash::SyncedPrintersManagerFactory::GetInstance());
}

ScalableIphFactory::~ScalableIphFactory() = default;

ScalableIphFactory* ScalableIphFactory::GetInstance() {
  static base::NoDestructor<ScalableIphFactory> instance;
  return instance.get();
}

scalable_iph::ScalableIph* ScalableIphFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<scalable_iph::ScalableIph*>(
      // Service must be created via `InitializeServiceForProfile`.
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 /*create=*/false));
}

void ScalableIphFactory::SetDelegateFactoryForTesting(
    DelegateTestingFactory delegate_testing_factory) {
  CHECK(delegate_testing_factory_.is_null())
      << "It's NOT allowed to set DelegateTestingFactory twice";

  delegate_testing_factory_ = std::move(delegate_testing_factory);
}

void ScalableIphFactory::InitializeServiceForProfile(Profile* profile) {
  // TODO(b/286604737): Disables ScalableIph services if multi-user sign-in is
  // used.

  // Create a `ScalableIph` service to start a timer for time tick event. Ignore
  // a return value. It can be nullptr if the browser context (i.e. profile) is
  // not eligible for `ScalableIph`.
  GetServiceForBrowserContext(profile, /*create=*/true);
}

content::BrowserContext* ScalableIphFactory::GetBrowserContextToUse(
    content::BrowserContext* browser_context) const {
  // TODO(b/286604737): Do not return a ScalableIph services if multi-user
  // sign-in is used.

  if (!ash::features::IsScalableIphEnabled()) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile) {
    return nullptr;
  }

  if (!profile->IsRegularProfile()) {
    return nullptr;
  }

  if (profile->IsChild()) {
    return nullptr;
  }

  if (IsSupportedEmailDomain(browser_context)) {
    return browser_context;
  }

  if (g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->IsDeviceEnterpriseManaged()) {
    return nullptr;
  }

  if (profile->GetProfilePolicyConnector()->IsManaged()) {
    return nullptr;
  }

  CHECK(user_manager::UserManager::IsInitialized())
      << "UserManager is required for an eligibility check";
  // Check that the user profile is the device owner, excepting when
  // the device owner id is not registered yet (i.e. first sessions).
  if (user_manager::UserManager::Get()->GetOwnerAccountId() !=
          EmptyAccountId() &&
      !user_manager::UserManager::Get()->IsOwnerUser(
          GetUser(browser_context))) {
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
    return delegate_testing_factory_.Run(profile);
  }

  return std::make_unique<ash::ScalableIphDelegateImpl>(profile);
}
