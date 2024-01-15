// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kcer/kcer_factory.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "chrome/browser/lacros/cert/cert_db_initializer_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/components/kcer/chaps/session_chaps_client.h"
#include "chromeos/components/kcer/extra_instances.h"
#include "chromeos/components/kcer/kcer.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceCallback;
using crosapi::mojom::ChapsService;
using crosapi::mojom::Crosapi;

// These browser tests test KcerFactory and KcerFactoryLacros. The factory is
// created outside of the tests by the code that also creates it in production.

namespace kcer {
namespace {

bool WeakPtrEq(const base::WeakPtr<kcer::Kcer>& v1,
               const base::WeakPtr<kcer::Kcer>& v2) {
  if (bool(v1) != bool(v2)) {
    return false;
  }
  return (v1.get() == v2.get());
}

class FakeCertDatabase : public crosapi::mojom::CertDatabase {
 public:
  FakeCertDatabase()
      : cert_db_info_(crosapi::mojom::GetCertDatabaseInfoResult::New()) {}

  void GetCertDatabaseInfo(GetCertDatabaseInfoCallback callback) override {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), cert_db_info_.Clone()));
  }

  void OnCertsChangedInLacros(
      crosapi::mojom::CertDatabaseChangeType change_type) override {}

  void AddAshCertDatabaseObserver(
      ::mojo::PendingRemote<crosapi::mojom::AshCertDatabaseObserver> observer)
      override {}

  void SetCertsProvidedByExtension(
      const std::string& extension_id,
      const std::vector<::chromeos::certificate_provider::CertificateInfo>&
          cert_infos) override {}

  mojo::Receiver<crosapi::mojom::CertDatabase>& GetReceiver() {
    return receiver_;
  }

  crosapi::mojom::GetCertDatabaseInfoResultPtr cert_db_info_;

 private:
  mojo::Receiver<crosapi::mojom::CertDatabase> receiver_{this};
};

class KcerFactoryNoNssTest : public InProcessBrowserTest {
 protected:
  KcerFactoryNoNssTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{kKcerWithoutNss}, /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    chromeos::LacrosService::Get()->InjectRemoteForTesting(
        fake_mojo_cert_db_.GetReceiver().BindNewPipeAndPassRemote());
  }

  // Skip some tests if ChapsService mojo API is not available. By the time
  // Kcer-without-NSS is enabled, the minimum supported version will always have
  // the API. The API was added in M-122, Kcer-without-NSS can be enabled in
  // M-124 (~Mar'24).
  bool ShouldSkip() {
    chromeos::LacrosService* service = chromeos::LacrosService::Get();
    const int required_version = static_cast<int>(
        Crosapi::MethodMinVersions::kBindChapsServiceMinVersion);
    if ((service->GetInterfaceVersion<Crosapi>() < required_version) &&
        !service->IsSupported<ChapsService>()) {
      return true;
    }
    return false;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  FakeCertDatabase fake_mojo_cert_db_;
};

// Test that ExtraInstances::GetDefaultKcer() returns the instance for the
// primary profile.
IN_PROC_BROWSER_TEST_F(KcerFactoryNoNssTest, DefaultKcerIsPrimaryProfileKcer) {
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();

  base::WeakPtr<Kcer> kcer = KcerFactory::GetKcer(primary_profile);

  base::WeakPtr<Kcer> default_kcer = ExtraInstances::GetDefaultKcer();

  ASSERT_TRUE(kcer);
  ASSERT_TRUE(default_kcer);
  EXPECT_EQ(kcer.get(), default_kcer.get());
}

// Test that KcerFactory can create an instance with both tokens.
IN_PROC_BROWSER_TEST_F(KcerFactoryNoNssTest, KcerWithBothTokensCreated) {
  if (ShouldSkip()) {
    GTEST_SKIP();
  }

  fake_mojo_cert_db_.cert_db_info_->should_load_chaps = true;
  fake_mojo_cert_db_.cert_db_info_->private_slot_id = 1;
  fake_mojo_cert_db_.cert_db_info_->enable_system_slot = true;
  fake_mojo_cert_db_.cert_db_info_->system_slot_id = 0;

  std::unique_ptr<TestingProfile> testing_profile =
      TestingProfile::Builder().Build();
  testing_profile->SetIsMainProfile(true);

  base::WeakPtr<Kcer> kcer = KcerFactory::GetKcer(testing_profile.get());
  ASSERT_TRUE(kcer);

  base::test::TestFuture<base::flat_set<Token>> tokens_waiter;
  kcer->GetAvailableTokens(tokens_waiter.GetCallback());
  EXPECT_EQ(tokens_waiter.Get(),
            base::flat_set<Token>({Token::kUser, Token::kDevice}));
  // The factory is responsible for initializing HighLevelChapsClient.
  EXPECT_TRUE(KcerFactory::IsHighLevelChapsClientInitialized());
}

// Test that KcerFactory can create an instance with one token.
IN_PROC_BROWSER_TEST_F(KcerFactoryNoNssTest, KcerWithOneTokensCreated) {
  if (ShouldSkip()) {
    GTEST_SKIP();
  }

  fake_mojo_cert_db_.cert_db_info_->should_load_chaps = true;
  fake_mojo_cert_db_.cert_db_info_->private_slot_id = 1;
  fake_mojo_cert_db_.cert_db_info_->enable_system_slot = false;

  std::unique_ptr<TestingProfile> testing_profile =
      TestingProfile::Builder().Build();
  testing_profile->SetIsMainProfile(true);

  base::WeakPtr<Kcer> kcer = KcerFactory::GetKcer(testing_profile.get());
  ASSERT_TRUE(kcer);

  base::test::TestFuture<base::flat_set<Token>> tokens_waiter;
  kcer->GetAvailableTokens(tokens_waiter.GetCallback());
  EXPECT_EQ(tokens_waiter.Get(), base::flat_set<Token>({Token::kUser}));
  // The factory is responsible for initializing HighLevelChapsClient.
  EXPECT_TRUE(KcerFactory::IsHighLevelChapsClientInitialized());
}

// Test that KcerFactory redirects off-the-record profile to their regular
// profiles.
IN_PROC_BROWSER_TEST_F(KcerFactoryNoNssTest, OffTheRecordProfileIsRedirected) {
  std::unique_ptr<TestingProfile> testing_profile =
      TestingProfile::Builder().Build();
  testing_profile->SetIsMainProfile(true);
  Profile* off_the_record_profile = testing_profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);

  base::WeakPtr<Kcer> kcer = KcerFactory::GetKcer(testing_profile.get());
  base::WeakPtr<Kcer> off_the_record_kcer =
      KcerFactory::GetKcer(off_the_record_profile);

  EXPECT_TRUE(WeakPtrEq(kcer, off_the_record_kcer));
}

// Test ExtraInstances::GetEmptyKcer() returns an instance of Kcer that
// doesn't have any tokens.
IN_PROC_BROWSER_TEST_F(KcerFactoryNoNssTest,
                       EmptySpecialInstanceDoesNotHaveTokens) {
  base::WeakPtr<Kcer> kcer = ExtraInstances::GetEmptyKcer();

  base::test::TestFuture<base::flat_set<Token>> tokens_waiter;
  kcer->GetAvailableTokens(tokens_waiter.GetCallback());
  EXPECT_EQ(tokens_waiter.Get(), base::flat_set<Token>({}));
}

// Test that device Kcer has correct tokens in Lacros.
IN_PROC_BROWSER_TEST_F(KcerFactoryNoNssTest, DeviceKcerHasCorrectTokens) {
  base::WeakPtr<Kcer> kcer = ExtraInstances::GetDeviceKcer();

  base::test::TestFuture<base::flat_set<Token>> tokens_waiter;
  kcer->GetAvailableTokens(tokens_waiter.GetCallback());

  EXPECT_EQ(tokens_waiter.Get(), base::flat_set<Token>({}));
}

// Test that for a system profile the factory returns Kcer without any tokens.
IN_PROC_BROWSER_TEST_F(KcerFactoryNoNssTest, SystemProfileHasNoTokens) {
  std::unique_ptr<TestingProfile> testing_profile =
      TestingProfile::Builder()
          .SetPath(ProfileManager::GetSystemProfilePath())
          .Build();
  ASSERT_TRUE(testing_profile->IsSystemProfile());

  base::WeakPtr<Kcer> kcer = KcerFactory::GetKcer(testing_profile.get());

  EXPECT_TRUE(WeakPtrEq(kcer, ExtraInstances::GetEmptyKcer()));
}

}  // namespace
}  // namespace kcer
