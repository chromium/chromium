// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_test_environment.h"

#include "chrome/browser/glic/auth_controller.h"
#include "chrome/browser/glic/glic_cookie_synchronizer.h"
#include "chrome/browser/glic/glic_fre_controller.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"

namespace glic {
namespace internal {

// A fake GlicCookieSynchronizer.
class TestCookieSynchronizer : public glic::GlicCookieSynchronizer {
 public:
  static TestCookieSynchronizer* InjectForProfile(Profile* profile) {
    GlicKeyedService* service =
        GlicKeyedServiceFactory::GetGlicKeyedService(profile);
    auto cookie_synchronizer = std::make_unique<TestCookieSynchronizer>(
        profile, IdentityManagerFactory::GetForProfile(profile),
        /*for_fre=*/false);
    TestCookieSynchronizer* ptr = cookie_synchronizer.get();
    service->GetAuthController().SetCookieSynchronizerForTesting(
        std::move(cookie_synchronizer));

    service->window_controller()
        .fre_controller()
        ->GetAuthControllerForTesting()
        .SetCookieSynchronizerForTesting(
            std::make_unique<TestCookieSynchronizer>(
                profile, IdentityManagerFactory::GetForProfile(profile),
                /*for_fre=*/true));

    return ptr;
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

GlicTestEnvironment::GlicTestEnvironment(Profile* profile) {
  cookie_synchronizer_ =
      internal::TestCookieSynchronizer::InjectForProfile(profile)->GetWeakPtr();
  ForceSigninAndModelExecutionCapability(profile);
}

GlicTestEnvironment::~GlicTestEnvironment() = default;

void GlicTestEnvironment::SetResultForFutureCookieSyncRequests(bool result) {
  cookie_synchronizer_->set_copy_cookies_result(result);
}

}  // namespace glic
