// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/glic_test_environment.h"

#include "base/no_destructor.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/glic_cookie_synchronizer.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

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
    service->window_controller()
        .fre_controller()
        ->GetAuthControllerForTesting()
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

GlicTestEnvironment::GlicTestEnvironment(
    const GlicTestEnvironmentConfig& config,
    std::vector<base::test::FeatureRef> enabled_features,
    std::vector<base::test::FeatureRef> disabled_features) {
  internal::GetConfig() = config;

  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

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
    SigninWithPrimaryAccount(profile);
    SetModelExecutionCapability(profile, true);
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

}  // namespace glic
