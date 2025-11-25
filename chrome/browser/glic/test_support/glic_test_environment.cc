// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/glic_test_environment.h"

#include "base/no_destructor.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/glic_cookie_synchronizer.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace glic {

namespace internal {
GlicTestEnvironmentConfig& GetConfig() {
  static GlicTestEnvironmentConfig config;
  return config;
}

// A fake GlicCookieSynchronizer.
class TestCookieSynchronizer : public glic::GlicCookieSynchronizer {
 public:
  static std::pair<TestCookieSynchronizer*, TestCookieSynchronizer*>
  InjectForProfile(Profile* profile) {
    GlicKeyedService* service =
        GlicKeyedServiceFactory::GetGlicKeyedService(profile, true);
    auto cookie_synchronizer = std::make_unique<TestCookieSynchronizer>(
        profile, IdentityManagerFactory::GetForProfile(profile),
        /*for_fre=*/false);
    TestCookieSynchronizer* ptr = cookie_synchronizer.get();
    service->GetAuthController().SetCookieSynchronizerForTesting(
        std::move(cookie_synchronizer));

    auto fre_cookie_synchronizer = std::make_unique<TestCookieSynchronizer>(
        profile, IdentityManagerFactory::GetForProfile(profile),
        /*for_fre=*/true);
    TestCookieSynchronizer* fre_cookie_synchronizer_ptr =
        fre_cookie_synchronizer.get();
    service->fre_controller()
        .GetAuthControllerForTesting()
        .SetCookieSynchronizerForTesting(std::move(fre_cookie_synchronizer));

    return std::make_pair(ptr, fre_cookie_synchronizer_ptr);
  }

  using GlicCookieSynchronizer::GlicCookieSynchronizer;

  void CopyCookiesToWebviewStoragePartition(
      base::OnceCallback<void(bool)> callback) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), copy_cookies_result_));
  }

  void set_copy_cookies_result(bool result) { copy_cookies_result_ = result; }
  base::WeakPtr<TestCookieSynchronizer> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  bool copy_cookies_result_ = true;

  base::WeakPtrFactory<TestCookieSynchronizer> weak_ptr_factory_{this};
};

class GlicTestEnvironmentServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static GlicTestEnvironmentService* GetForProfile(Profile* profile,
                                                   bool create) {
    return static_cast<GlicTestEnvironmentService*>(
        GetInstance()->GetServiceForBrowserContext(profile, create));
  }
  static GlicTestEnvironmentServiceFactory* GetInstance() {
    static base::NoDestructor<GlicTestEnvironmentServiceFactory> instance;
    return instance.get();
  }

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override {
    return std::make_unique<GlicTestEnvironmentService>(
        Profile::FromBrowserContext(context));
  }

 private:
  friend class base::NoDestructor<GlicTestEnvironmentServiceFactory>;

  GlicTestEnvironmentServiceFactory()
      : ProfileKeyedServiceFactory(
            "GlicTestEnvironmentService",
            ProfileSelections::BuildForRegularProfile()) {
    // It would be sensible to depend on GlicKeyedServiceFactory, but that ends
    // up creating some service factories too early.
  }
  ~GlicTestEnvironmentServiceFactory() override = default;
};

}  // namespace internal

std::vector<base::test::FeatureRef> GetDefaultEnabledGlicTestFeatures() {
  return {features::kGlic, features::kTabstripComboButton,
          features::kGlicRollout,
#if BUILDFLAG(IS_CHROMEOS)
          chromeos::features::kFeatureManagementGlic
#endif  // BUILDFLAG(IS_CHROMEOS)
  };
}
std::vector<base::test::FeatureRef> GetDefaultDisabledGlicTestFeatures() {
  return {features::kGlicWarming, features::kGlicFreWarming};
}

GlicTestEnvironment::GlicTestEnvironment(
    const GlicTestEnvironmentConfig& config,
    std::vector<base::test::FeatureRef> enabled_features,
    std::vector<base::test::FeatureRef> disabled_features)
    : shared_(enabled_features, disabled_features) {
  internal::GetConfig() = config;

  // The service factory needs to be created before any services are created.
  internal::GlicTestEnvironmentServiceFactory::GetInstance();
  create_services_subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
              &GlicTestEnvironment::OnWillCreateBrowserContextKeyedServices,
              base::Unretained(this)));
}

GlicTestEnvironment::~GlicTestEnvironment() = default;

void GlicTestEnvironment::SetForceSigninAndModelExecutionCapability(
    bool force) {
  internal::GetConfig().force_signin_and_model_execution_capability = force;
}

void GlicTestEnvironment::SetFreStatusForNewProfiles(
    std::optional<prefs::FreStatus> fre_status) {
  internal::GetConfig().fre_status = fre_status;
}

GlicTestEnvironmentService* GlicTestEnvironment::GetService(Profile* profile,
                                                            bool create) {
  return internal::GlicTestEnvironmentServiceFactory::GetForProfile(profile,
                                                                    create);
}

void GlicTestEnvironmentService::SetModelExecutionCapability(bool enabled) {
  ::glic::SetModelExecutionCapability(profile_, enabled);
}

void GlicTestEnvironment::OnWillCreateBrowserContextKeyedServices(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile || !GlicEnabling::IsProfileEligible(profile)) {
    LOG(WARNING) << "Not creating GlicTestEnvironmentService for "
                    "ineligible profile.";
    return;
  }
  if (internal::GetConfig().force_signin_and_model_execution_capability) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  auto observation =
      std::make_unique<base::ScopedObservation<Profile, ProfileObserver>>(this);
  observation->Observe(profile);
  profile_observations_.push_back(std::move(observation));
}

void GlicTestEnvironment::OnProfileWillBeDestroyed(Profile* profile) {
  std::erase_if(profile_observations_, [&profile](const auto& observation) {
    return observation->GetSource() == profile;
  });
}

void GlicTestEnvironment::OnProfileInitializationComplete(Profile* profile) {
  GetService(profile, true);
}

GlicTestEnvironmentService::GlicTestEnvironmentService(Profile* profile)
    : profile_(profile) {
  std::pair<internal::TestCookieSynchronizer*,
            internal::TestCookieSynchronizer*>
      cookie_synchronizers =
          internal::TestCookieSynchronizer::InjectForProfile(profile);

  cookie_synchronizer_ = cookie_synchronizers.first->GetWeakPtr();
  fre_cookie_synchronizer_ = cookie_synchronizers.second->GetWeakPtr();
  const GlicTestEnvironmentConfig& config = internal::GetConfig();
  if (config.fre_status) {
    SetFRECompletion(*config.fre_status);
  }
  if (config.force_signin_and_model_execution_capability) {
#if BUILDFLAG(IS_CHROMEOS)
    // SigninWithPrimaryAccount below internally runs RunLoop to wait for an
    // async task completion. This is the test only behavior.
    // However, that has side effect that other tasks in the queue also runs.
    // Specifically, some of the queued tasks will require communicating with
    // SigninBrowserContext, and if it's not existing, the instance is created.
    // Because this running inside a KeyedService construction,
    // KeyedServiceTemplatedFactory temporarily keeps the map entry between
    // the current browser context and the info of this KeyedService instance.
    // However, the entry is invalidated if another BrowserContext is created,
    // like SigninBrowserContext case explained above, regardless of whether
    // the newly created BrowserContext will create this service or not.
    // Thus, after returning from this function, it will cause crashes.
    // To avoid such crashes, while running the async task, we temporarily
    // disable the creation of new BrowserContext for the workaround.
    // See also crbug.com/460334478 for details.
    auto disabled = ash::BrowserContextHelper::
        DisableImplicitBrowserContextCreationForTest();
#endif
    SigninWithPrimaryAccount(profile);
    SetModelExecutionCapability(true);
  }
}

GlicTestEnvironmentService::~GlicTestEnvironmentService() = default;

GlicKeyedService* GlicTestEnvironmentService::GetService() {
  return GlicKeyedServiceFactory::GetGlicKeyedService(profile_);
}

void GlicTestEnvironmentService::SetResultForFutureCookieSync(bool result) {
  cookie_synchronizer_->set_copy_cookies_result(result);
}

void GlicTestEnvironmentService::SetResultForFutureCookieSyncInFre(
    bool result) {
  fre_cookie_synchronizer_->set_copy_cookies_result(result);
}

void GlicTestEnvironmentService::SetFRECompletion(prefs::FreStatus fre_status) {
  ::glic::SetFRECompletion(profile_, fre_status);
}

GlicUnitTestEnvironment::GlicUnitTestEnvironment(
    const GlicTestEnvironmentConfig& config)
    : config_(config),
      shared_(GetDefaultEnabledGlicTestFeatures(),
              GetDefaultDisabledGlicTestFeatures()) {}

GlicUnitTestEnvironment::~GlicUnitTestEnvironment() = default;

void GlicUnitTestEnvironment::SetupProfile(Profile* profile) {
  if (config_.force_signin_and_model_execution_capability) {
    ::glic::ForceSigninAndModelExecutionCapability(profile);
  }
  if (config_.fre_status) {
    glic::SetFRECompletion(profile, *config_.fre_status);
  }
}

internal::GlicTestEnvironmentShared::GlicTestEnvironmentShared(
    std::vector<base::test::FeatureRef> enabled_features,
    std::vector<base::test::FeatureRef> disabled_features) {
  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
}

}  // namespace glic
