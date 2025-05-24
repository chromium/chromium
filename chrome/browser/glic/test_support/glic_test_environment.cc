// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/glic_test_environment.h"

#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/glic_cookie_synchronizer.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"

namespace glic {
namespace internal {

// A fake GlicCookieSynchronizer.
class TestCookieSynchronizer : public glic::GlicCookieSynchronizer {
 public:
  static std::pair<TestCookieSynchronizer*, TestCookieSynchronizer*>
  InjectForProfile(Profile* profile) {
    GlicKeyedService* service =
        GlicKeyedServiceFactory::GetGlicKeyedService(profile);
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

}  // namespace internal

GlicTestEnvironment::GlicTestEnvironment(Profile* profile) : profile_(profile) {
  std::pair<internal::TestCookieSynchronizer*,
            internal::TestCookieSynchronizer*>
      cookie_synchronizers =
          internal::TestCookieSynchronizer::InjectForProfile(profile);

  cookie_synchronizer_ = cookie_synchronizers.first->GetWeakPtr();
  fre_cookie_synchronizer_ = cookie_synchronizers.second->GetWeakPtr();
  ForceSigninAndModelExecutionCapability(profile);
}

GlicTestEnvironment::~GlicTestEnvironment() = default;

GlicKeyedService* GlicTestEnvironment::GetService() {
  return GlicKeyedServiceFactory::GetGlicKeyedService(profile_);
}

void GlicTestEnvironment::SetResultForFutureCookieSync(bool result) {
  cookie_synchronizer_->set_copy_cookies_result(result);
}

void GlicTestEnvironment::SetResultForFutureCookieSyncInFre(bool result) {
  fre_cookie_synchronizer_->set_copy_cookies_result(result);
}

void GlicTestEnvironment::SetFRECompletion(prefs::FreStatus fre_status) {
  ::glic::SetFRECompletion(profile_, fre_status);
}

}  // namespace glic
