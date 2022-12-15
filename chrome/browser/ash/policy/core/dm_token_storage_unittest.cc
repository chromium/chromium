// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/dm_token_storage.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using ash::FakeCryptohomeMiscClient;

namespace policy {

class DMTokenStorageTest : public testing::Test {
 public:
  DMTokenStorageTest()
      : scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()) {}
  ~DMTokenStorageTest() override {}

  void SetSaltPending() {
    // Clear the cached salt.
    ash::SystemSaltGetter::Shutdown();
    FakeCryptohomeMiscClient::Get()->set_system_salt(std::vector<uint8_t>());
    FakeCryptohomeMiscClient::Get()->SetServiceIsAvailable(false);
    ash::SystemSaltGetter::Initialize();
  }

  void SetSaltAvailable() {
    FakeCryptohomeMiscClient::Get()->set_system_salt(
        FakeCryptohomeMiscClient::GetStubSystemSalt());
    FakeCryptohomeMiscClient::Get()->SetServiceIsAvailable(true);
  }

  void SetSaltError() {
    FakeCryptohomeMiscClient::Get()->set_system_salt(std::vector<uint8_t>());
    FakeCryptohomeMiscClient::Get()->SetServiceIsAvailable(true);
  }

  void SetUp() override {
    ash::CryptohomeMiscClient::InitializeFake();
    SetSaltAvailable();

    ash::SystemSaltGetter::Initialize();
  }

  void TearDown() override {
    dm_token_storage_.reset();
    ash::SystemSaltGetter::Shutdown();
    ash::CryptohomeMiscClient::Shutdown();
    base::RunLoop().RunUntilIdle();
  }

  void CreateDMStorage() {
    dm_token_storage_ =
        std::make_unique<DMTokenStorage>(scoped_testing_local_state_.Get());
  }

  void StoreDMToken() {
    base::RunLoop run_loop;
    dm_token_storage_->StoreDMToken(
        "test-token",
        base::BindOnce(&DMTokenStorageTest::OnStoreCallback,
                       base::Unretained(this), run_loop.QuitClosure(), true));
    run_loop.Run();
  }

  void OnStoreCallback(base::OnceClosure closure, bool expected, bool success) {
    EXPECT_EQ(expected, success);
    if (!closure.is_null())
      std::move(closure).Run();
  }

  void OnRetrieveCallback(base::OnceClosure closure,
                          const std::string& expected,
                          const std::string& actual) {
    EXPECT_EQ(expected, actual);
    if (!closure.is_null())
      std::move(closure).Run();
  }

  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<DMTokenStorage> dm_token_storage_;
};

TEST_F(DMTokenStorageTest, SaveEncryptedToken) {
  CreateDMStorage();
  StoreDMToken();

  {
    base::RunLoop run_loop;
    dm_token_storage_->RetrieveDMToken(base::BindOnce(
        &DMTokenStorageTest::OnRetrieveCallback, base::Unretained(this),
        run_loop.QuitClosure(), "test-token"));
    run_loop.Run();
  }
  // Reload shouldn't change the token.
  CreateDMStorage();
  {
    base::RunLoop run_loop;
    dm_token_storage_->RetrieveDMToken(base::BindOnce(
        &DMTokenStorageTest::OnRetrieveCallback, base::Unretained(this),
        run_loop.QuitClosure(), "test-token"));
    run_loop.Run();
  }
  {
    // Subsequent retrieving DM token should succeed.
    base::RunLoop run_loop;
    dm_token_storage_->RetrieveDMToken(base::BindOnce(
        &DMTokenStorageTest::OnRetrieveCallback, base::Unretained(this),
        run_loop.QuitClosure(), "test-token"));
    run_loop.Run();
  }
}

TEST_F(DMTokenStorageTest, RetrieveEncryptedTokenWithPendingSalt) {
  CreateDMStorage();
  StoreDMToken();

  SetSaltPending();
  CreateDMStorage();

  {
    base::RunLoop run_loop;
    dm_token_storage_->RetrieveDMToken(base::BindOnce(
        &DMTokenStorageTest::OnRetrieveCallback, base::Unretained(this),
        run_loop.QuitClosure(), "test-token"));
    SetSaltAvailable();
    run_loop.Run();
  }
}

TEST_F(DMTokenStorageTest, StoreEncryptedTokenWithPendingSalt) {
  SetSaltPending();
  CreateDMStorage();
  base::RunLoop run_loop;
  dm_token_storage_->StoreDMToken(
      "test-token",
      base::BindOnce(&DMTokenStorageTest::OnStoreCallback,
                     base::Unretained(this), run_loop.QuitClosure(), true));
  SetSaltAvailable();
  run_loop.Run();
}

TEST_F(DMTokenStorageTest, MultipleRetrieveTokenCalls) {
  CreateDMStorage();
  StoreDMToken();
  {
    base::RunLoop run_loop;
    for (int i = 0; i < 3; ++i) {
      dm_token_storage_->RetrieveDMToken(base::BindOnce(
          &DMTokenStorageTest::OnRetrieveCallback, base::Unretained(this),
          run_loop.QuitClosure(), "test-token"));
    }
    run_loop.Run();
  }
}

TEST_F(DMTokenStorageTest, StoreWithSaltError) {
  SetSaltError();
  CreateDMStorage();
  base::RunLoop run_loop;
  dm_token_storage_->StoreDMToken(
      "test-token",
      base::BindOnce(&DMTokenStorageTest::OnStoreCallback,
                     base::Unretained(this), run_loop.QuitClosure(), false));
  run_loop.Run();
}

TEST_F(DMTokenStorageTest, RetrieveWithSaltError) {
  CreateDMStorage();
  StoreDMToken();
  SetSaltPending();
  CreateDMStorage();
  base::RunLoop run_loop;
  dm_token_storage_->RetrieveDMToken(
      base::BindOnce(&DMTokenStorageTest::OnRetrieveCallback,
                     base::Unretained(this), run_loop.QuitClosure(), ""));
  SetSaltError();
  run_loop.Run();
}

TEST_F(DMTokenStorageTest, RetrieveWithNoToken) {
  CreateDMStorage();
  base::RunLoop run_loop;
  dm_token_storage_->RetrieveDMToken(
      base::BindOnce(&DMTokenStorageTest::OnRetrieveCallback,
                     base::Unretained(this), run_loop.QuitClosure(), ""));
  run_loop.Run();
}

TEST_F(DMTokenStorageTest, RetrieveFailIfStoreRunning) {
  SetSaltPending();
  CreateDMStorage();
  base::RunLoop run_loop;
  dm_token_storage_->StoreDMToken(
      "test-token",
      base::BindOnce(&DMTokenStorageTest::OnStoreCallback,
                     base::Unretained(this), run_loop.QuitClosure(), true));
  dm_token_storage_->RetrieveDMToken(
      base::BindOnce(&DMTokenStorageTest::OnRetrieveCallback,
                     base::Unretained(this), base::OnceClosure(), ""));
  SetSaltAvailable();
  run_loop.Run();
}

TEST_F(DMTokenStorageTest, StoreFailIfAnotherStoreRunning) {
  SetSaltPending();
  CreateDMStorage();
  base::RunLoop run_loop;
  dm_token_storage_->StoreDMToken(
      "test-token",
      base::BindOnce(&DMTokenStorageTest::OnStoreCallback,
                     base::Unretained(this), run_loop.QuitClosure(), true));
  dm_token_storage_->StoreDMToken(
      "test-token",
      base::BindOnce(&DMTokenStorageTest::OnStoreCallback,
                     base::Unretained(this), base::OnceClosure(), false));
  SetSaltAvailable();
  run_loop.Run();
}

}  // namespace policy
