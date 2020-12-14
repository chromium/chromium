// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_token_cert_db_initializer.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/tpm/tpm_token_loader.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "net/cert/nss_cert_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

// A helper that wraps the callback passed to
// SystemTokenCertDBInitializer::GetSystemTokenCertDb and can answer queries
// regarding the state of the callback and database passed to the callback.
class GetSystemTokenCertDbCallbackWrapper {
 public:
  GetSystemTokenCertDbCallbackWrapper() = default;
  GetSystemTokenCertDbCallbackWrapper(
      const GetSystemTokenCertDbCallbackWrapper& other) = delete;
  GetSystemTokenCertDbCallbackWrapper& operator=(
      const GetSystemTokenCertDbCallbackWrapper& other) = delete;
  ~GetSystemTokenCertDbCallbackWrapper() = default;

  SystemTokenCertDBInitializer::GetSystemTokenCertDbCallback GetCallback() {
    return base::BindOnce(&GetSystemTokenCertDbCallbackWrapper::OnDbRetrieved,
                          weak_ptr_factory_.GetWeakPtr());
  }

  // Waits until the callback returned by GetCallback() has been called.
  void Wait() { run_loop_.Run(); }

  bool IsCallbackCalled() { return done_; }
  bool IsDbRetrievalSucceeded() { return nss_cert_database_ != nullptr; }

 private:
  void OnDbRetrieved(net::NSSCertDatabase* nss_cert_database) {
    EXPECT_FALSE(done_);
    done_ = true;
    nss_cert_database_ = nss_cert_database;
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  bool done_ = false;
  net::NSSCertDatabase* nss_cert_database_ = nullptr;

  base::WeakPtrFactory<GetSystemTokenCertDbCallbackWrapper> weak_ptr_factory_{
      this};
};

}  // namespace

class SystemTokenCertDbInitializerTest : public testing::Test {
 public:
  SystemTokenCertDbInitializerTest() {
    TPMTokenLoader::InitializeForTest();
    CryptohomeClient::InitializeFake();
    NetworkCertLoader::Initialize();
    TpmManagerClient::InitializeFake();

    system_token_cert_db_initializer_ =
        std::make_unique<SystemTokenCertDBInitializer>();
  }

  SystemTokenCertDbInitializerTest(
      const SystemTokenCertDbInitializerTest& other) = delete;
  SystemTokenCertDbInitializerTest& operator=(
      const SystemTokenCertDbInitializerTest& other) = delete;

  ~SystemTokenCertDbInitializerTest() override {
    TpmManagerClient::Shutdown();
    NetworkCertLoader::Shutdown();
    CryptohomeClient::Shutdown();
    TPMTokenLoader::Shutdown();
  }

 protected:
  bool InitializeTestSystemSlot() {
    test_system_slot_ = std::make_unique<crypto::ScopedTestSystemNSSKeySlot>();
    return test_system_slot_->ConstructedSuccessfully();
  }

  SystemTokenCertDBInitializer* system_token_cert_db_initializer() {
    return system_token_cert_db_initializer_.get();
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<SystemTokenCertDBInitializer>
      system_token_cert_db_initializer_;
  std::unique_ptr<crypto::ScopedTestSystemNSSKeySlot> test_system_slot_;
};

// Tests that the system token certificate database will be returned
// successfully by SystemTokenCertDbInitializer if it was available in less than
// 5 minutes after being requested.
TEST_F(SystemTokenCertDbInitializerTest, GetSystemTokenCertDbSuccess) {
  GetSystemTokenCertDbCallbackWrapper get_system_token_cert_db_callback_wrapper;
  system_token_cert_db_initializer()->GetSystemTokenCertDb(
      get_system_token_cert_db_callback_wrapper.GetCallback());
  EXPECT_FALSE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());

  // Check that after 1 minute, SystemTokenCertDBInitializer is still waiting
  // for the system token slot to be initialized and the DB retrieval hasn't
  // timed out yet.
  const auto kOneMinuteDelay = base::TimeDelta::FromMinutes(1);
  EXPECT_LT(kOneMinuteDelay,
            SystemTokenCertDBInitializer::kMaxCertDbRetrievalDelay);

  task_environment()->FastForwardBy(kOneMinuteDelay);
  EXPECT_FALSE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());

  EXPECT_TRUE(InitializeTestSystemSlot());
  get_system_token_cert_db_callback_wrapper.Wait();

  EXPECT_TRUE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());
  EXPECT_TRUE(
      get_system_token_cert_db_callback_wrapper.IsDbRetrievalSucceeded());
}

// Tests that the system token certificate database will be returned
// successfully by SystemTokenCertDbInitializer if it was available in less than
// 5 minutes after being requested, and the system slot uses software fallback
// when it's allowed and TPM is disabled.
TEST_F(SystemTokenCertDbInitializerTest,
       GetSystemTokenCertDbSuccessSoftwareFallback) {
  TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_nonsensitive_status_reply()
      ->set_is_enabled(false);
  TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_nonsensitive_status_reply()
      ->set_is_owned(false);
  system_token_cert_db_initializer()
      ->set_is_system_slot_software_fallback_allowed(true);

  GetSystemTokenCertDbCallbackWrapper get_system_token_cert_db_callback_wrapper;
  system_token_cert_db_initializer()->GetSystemTokenCertDb(
      get_system_token_cert_db_callback_wrapper.GetCallback());
  EXPECT_FALSE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());

  // Check that after 1 minute, SystemTokenCertDBInitializer is still waiting
  // for the system token slot to be initialized and the DB retrieval hasn't
  // timed out yet.
  const auto kOneMinuteDelay = base::TimeDelta::FromMinutes(1);
  EXPECT_LT(kOneMinuteDelay,
            SystemTokenCertDBInitializer::kMaxCertDbRetrievalDelay);

  task_environment()->FastForwardBy(kOneMinuteDelay);
  EXPECT_FALSE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());

  EXPECT_TRUE(InitializeTestSystemSlot());
  get_system_token_cert_db_callback_wrapper.Wait();

  EXPECT_TRUE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());
  EXPECT_TRUE(
      get_system_token_cert_db_callback_wrapper.IsDbRetrievalSucceeded());
}

// Tests that the system token certificate database will be not returned
// successfully by SystemTokenCertDbInitializer if TPM is disabled and system
// slot software fallback is not allowed.
TEST_F(SystemTokenCertDbInitializerTest,
       GetSystemTokenCertDbFailureDisabledTPM) {
  TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_nonsensitive_status_reply()
      ->set_is_enabled(false);
  TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_nonsensitive_status_reply()
      ->set_is_owned(false);
  system_token_cert_db_initializer()
      ->set_is_system_slot_software_fallback_allowed(false);

  GetSystemTokenCertDbCallbackWrapper get_system_token_cert_db_callback_wrapper;
  system_token_cert_db_initializer()->GetSystemTokenCertDb(
      get_system_token_cert_db_callback_wrapper.GetCallback());
  EXPECT_FALSE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());

  // Check that after 1 minute, SystemTokenCertDBInitializer is still waiting
  // for the system token slot to be initialized and the DB retrieval hasn't
  // timed out yet.
  const auto kOneMinuteDelay = base::TimeDelta::FromMinutes(1);
  EXPECT_LT(kOneMinuteDelay,
            SystemTokenCertDBInitializer::kMaxCertDbRetrievalDelay);

  task_environment()->FastForwardBy(kOneMinuteDelay);
  EXPECT_FALSE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());

  EXPECT_TRUE(InitializeTestSystemSlot());
  get_system_token_cert_db_callback_wrapper.Wait();

  EXPECT_TRUE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());
  EXPECT_FALSE(
      get_system_token_cert_db_callback_wrapper.IsDbRetrievalSucceeded());
}

// Tests that the system token certificate database will be returned
// successfully by SystemTokenCertDbInitializer if it was available in less than
// 5 minutes after being requested even if the slot was available after more
// than 5 minutes from the initialization of SystemTokenCertDbInitializer.
TEST_F(SystemTokenCertDbInitializerTest,
       GetSystemTokenCertDbLateRequestSuccess) {
  // Simulate waiting for 6 minutes after the initialization of the
  // SystemTokenCertDbInitializer.
  const auto kSixMinuteDelay = base::TimeDelta::FromMinutes(6);
  EXPECT_GT(kSixMinuteDelay,
            SystemTokenCertDBInitializer::kMaxCertDbRetrievalDelay);
  task_environment()->FastForwardBy(kSixMinuteDelay);
  EXPECT_TRUE(InitializeTestSystemSlot());

  GetSystemTokenCertDbCallbackWrapper get_system_token_cert_db_callback_wrapper;
  system_token_cert_db_initializer()->GetSystemTokenCertDb(
      get_system_token_cert_db_callback_wrapper.GetCallback());
  get_system_token_cert_db_callback_wrapper.Wait();

  EXPECT_TRUE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());
  EXPECT_TRUE(
      get_system_token_cert_db_callback_wrapper.IsDbRetrievalSucceeded());
}

// Tests that the system token certificate database retrieval will fail if the
// system token initialization doesn't succeed within 5 minutes from the first
// database request.
TEST_F(SystemTokenCertDbInitializerTest, GetSystemTokenCertDbTimeout) {
  GetSystemTokenCertDbCallbackWrapper get_system_token_cert_db_callback_wrapper;
  system_token_cert_db_initializer()->GetSystemTokenCertDb(
      get_system_token_cert_db_callback_wrapper.GetCallback());
  EXPECT_FALSE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());

  const auto kDelay1 = base::TimeDelta::FromMinutes(2);
  EXPECT_LT(kDelay1, SystemTokenCertDBInitializer::kMaxCertDbRetrievalDelay);

  const auto kDelay2 =
      SystemTokenCertDBInitializer::kMaxCertDbRetrievalDelay - kDelay1;

  task_environment()->FastForwardBy(kDelay1);
  EXPECT_FALSE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());

  task_environment()->FastForwardBy(kDelay2);
  EXPECT_TRUE(get_system_token_cert_db_callback_wrapper.IsCallbackCalled());
  EXPECT_FALSE(
      get_system_token_cert_db_callback_wrapper.IsDbRetrievalSucceeded());
}

// Tests that if one of the system token certificate database requests timed
// out, following requests will fail as well.
TEST_F(SystemTokenCertDbInitializerTest,
       GetSystemTokenCertDbTimeoutMultipleRequests) {
  GetSystemTokenCertDbCallbackWrapper
      get_system_token_cert_db_callback_wrapper_1;
  system_token_cert_db_initializer()->GetSystemTokenCertDb(
      get_system_token_cert_db_callback_wrapper_1.GetCallback());
  EXPECT_FALSE(get_system_token_cert_db_callback_wrapper_1.IsCallbackCalled());

  task_environment()->FastForwardBy(
      SystemTokenCertDBInitializer::kMaxCertDbRetrievalDelay);
  EXPECT_TRUE(get_system_token_cert_db_callback_wrapper_1.IsCallbackCalled());
  EXPECT_FALSE(
      get_system_token_cert_db_callback_wrapper_1.IsDbRetrievalSucceeded());

  GetSystemTokenCertDbCallbackWrapper
      get_system_token_cert_db_callback_wrapper_2;
  system_token_cert_db_initializer()->GetSystemTokenCertDb(
      get_system_token_cert_db_callback_wrapper_2.GetCallback());
  EXPECT_TRUE(get_system_token_cert_db_callback_wrapper_2.IsCallbackCalled());
  EXPECT_FALSE(
      get_system_token_cert_db_callback_wrapper_2.IsDbRetrievalSucceeded());
}

}  // namespace chromeos
