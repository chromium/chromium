// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scalable_iph/scalable_iph_factory_impl.h"

#include "ash/constants/ash_features.h"
#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/phonehub/phone_hub_manager_factory.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/printing/synced_printers_manager_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/scalable_iph/scalable_iph_delegate_impl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/ash/components/scalable_iph/logger.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_factory.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace {

enum class Error { kFail };

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

base::expected<bool, Error> IsMinor(content::BrowserContext* browser_context,
                                    Profile* profile) {
  const user_manager::User* user = GetUser(browser_context);
  if (!user) {
    return base::unexpected(Error::kFail);
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return base::unexpected(Error::kFail);
  }

  AccountInfo account_info = identity_manager->FindExtendedAccountInfoByGaiaId(
      user->GetAccountId().GetGaiaId());
  // Using `can_use_manta_service` as a signal to see if an account is minor or
  // not. This behavior is aligned with `CampaignsMatcher::MatchMinorUser`.
  // TODO(b/333896450): find a better signal for minor mode.
  signin::Tribool can_use_manta_service =
      account_info.capabilities.can_use_manta_service();
  bool is_minor = can_use_manta_service != signin::Tribool::kTrue;
  return is_minor;
}

}  // namespace

ScalableIphFactoryImpl::ScalableIphFactoryImpl() {
  CHECK(delegate_testing_factory_.is_null())
      << "Testing factory must be null at initialization.";

  // `ScalableIphFactoryImpl` is used to separate out dependencies
  // (such as `TrackerFactory`) from the `ScalableIphFactory` class
  // to avoid circular dependencies from //chrome/browser.
  DependsOn(feature_engagement::TrackerFactory::GetInstance());
  DependsOn(ash::SyncedPrintersManagerFactory::GetInstance());
  DependsOn(ash::phonehub::PhoneHubManagerFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

ScalableIphFactoryImpl::~ScalableIphFactoryImpl() = default;

void ScalableIphFactoryImpl::BuildInstance() {
  static base::NoDestructor<ScalableIphFactoryImpl> scalable_iph_factory_impl;
}

bool ScalableIphFactoryImpl::IsBrowserContextEligible(
    content::BrowserContext* browser_context) {
  return static_cast<ScalableIphFactoryImpl*>(GetInstance())
             ->GetBrowserContextToUse(browser_context) != nullptr;
}

void ScalableIphFactoryImpl::SetDelegateFactoryForTesting(
    DelegateTestingFactory delegate_testing_factory) {
  CHECK(delegate_testing_factory_.is_null())
      << "It's NOT allowed to set DelegateTestingFactory twice";

  delegate_testing_factory_ = std::move(delegate_testing_factory);
}

content::BrowserContext* ScalableIphFactoryImpl::GetBrowserContextToUseForDebug(
    content::BrowserContext* browser_context,
    scalable_iph::Logger* logger) const {
  return GetBrowserContextToUseInternal(browser_context, logger);
}

content::BrowserContext* ScalableIphFactoryImpl::GetBrowserContextToUse(
    content::BrowserContext* browser_context) const {
  scalable_iph::Logger logger_unused;
  return GetBrowserContextToUseInternal(browser_context, &logger_unused);
}

std::unique_ptr<KeyedService>
ScalableIphFactoryImpl::BuildServiceInstanceForBrowserContext(
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

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CHECK(identity_manager)
      << "No identity manager. This method cannot handle this error. "
         "BuildServiceInstanceForBrowserContext method is not allowed to "
         "return nullptr";

  std::unique_ptr<scalable_iph::Logger> logger =
      std::make_unique<scalable_iph::Logger>();
  std::unique_ptr<scalable_iph::ScalableIphDelegate> scalable_iph_delegate =
      CreateScalableIphDelegate(profile, logger.get());
  return std::make_unique<scalable_iph::ScalableIph>(
      tracker, std::move(scalable_iph_delegate), std::move(logger));
}

content::BrowserContext* ScalableIphFactoryImpl::GetBrowserContextToUseInternal(
    content::BrowserContext* browser_context,
    scalable_iph::Logger* logger) const {
  // TODO(b/286604737): Do not return a ScalableIph services if multi-user
  // sign-in is used.

  if (!ash::features::IsScalableIphEnabled()) {
    SCALABLE_IPH_LOG(logger) << "ScalableIph flag is off.";
    return nullptr;
  }

  if (!scalable_iph::ScalableIph::IsAnyIphFeatureEnabled()) {
    SCALABLE_IPH_LOG(logger) << "No iph feature is enabled.";
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile) {
    SCALABLE_IPH_LOG(logger) << "Unable to obtain a profile from a browser "
                                "context. browser_context==nullptr: "
                             << (browser_context == nullptr);
    return nullptr;
  }

  SCALABLE_IPH_LOG(logger) << "Profile user name: "
                           << profile->GetProfileUserName();

  if (!ash::IsUserBrowserContext(browser_context)) {
    SCALABLE_IPH_LOG(logger) << "Not a user browser context.";
    return nullptr;
  }

  if (!profile->IsRegularProfile()) {
    SCALABLE_IPH_LOG(logger) << "Profile is not a regular profile.";
    return nullptr;
  }

  if (profile->IsChild()) {
    SCALABLE_IPH_LOG(logger) << "Profile is a child profile.";
    return nullptr;
  }

  if (!ash::multidevice_setup::IsFeatureAllowed(
          ash::multidevice_setup::mojom::Feature::kPhoneHub,
          profile->GetPrefs())) {
    SCALABLE_IPH_LOG(logger) << "Phone hub feature is disabled by a policy.";
    DLOG(WARNING) << "Phone hub feature is disabled by a policy. This is "
                     "expected only for test code as we are returning early "
                     "above if a profile is managed.";
    return nullptr;
  }

  if (IsSupportedEmailDomain(browser_context)) {
    SCALABLE_IPH_LOG(logger)
        << "Provided browser context is in a supported email domain. Returning "
           "early as an eligible profile.";
    return browser_context;
  }

  if (g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->IsDeviceEnterpriseManaged()) {
    SCALABLE_IPH_LOG(logger) << "Device is managed.";
    return nullptr;
  }

  if (profile->GetProfilePolicyConnector()->IsManaged()) {
    SCALABLE_IPH_LOG(logger) << "Profile is managed.";
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
    SCALABLE_IPH_LOG(logger) << "User is not an owner.";
    return nullptr;
  }

  // Use `base::expected` instead of `std::optional` to avoid implicit bool
  // conversion: https://abseil.io/tips/141.
  base::expected<bool, Error> maybe_is_minor =
      IsMinor(browser_context, profile);
  if (!maybe_is_minor.has_value()) {
    SCALABLE_IPH_LOG(logger) << "Failed to get IsMinor value. Treating as "
                                "not-eligible for fail-safe.";
    return nullptr;
  }

  if (maybe_is_minor.value()) {
    SCALABLE_IPH_LOG(logger) << "User is a minor.";
    return nullptr;
  }

  SCALABLE_IPH_LOG(logger)
      << "This browser context is eligible for ScalableIph.";

  return browser_context;
}

std::unique_ptr<scalable_iph::ScalableIphDelegate>
ScalableIphFactoryImpl::CreateScalableIphDelegate(
    Profile* profile,
    scalable_iph::Logger* logger) const {
  CHECK(profile) << "Profile must not be nullptr for this method";
  CHECK(logger);

  if (!delegate_testing_factory_.is_null()) {
    return delegate_testing_factory_.Run(profile, logger);
  }

  return std::make_unique<ash::ScalableIphDelegateImpl>(profile, logger);
}
