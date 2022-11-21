// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_api.h"

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {

class IdentityAPITest : public testing::Test {
 public:
  IdentityAPITest()
      : prefs_(base::SingleThreadTaskRunner::GetCurrentDefault()),
        event_router_(prefs_.profile(), prefs_.prefs()),
        api_(CreateIdentityAPI()) {
    // IdentityAPITest requires the extended account info callbacks to be fired
    // on account update/removal.
    identity_env_.EnableRemovalOfExtendedAccountInfo();
  }

  ~IdentityAPITest() override { api_->Shutdown(); }

  std::unique_ptr<IdentityAPI> CreateIdentityAPI() {
    return base::WrapUnique(new IdentityAPI(prefs_.profile(),
                                            identity_env_.identity_manager(),
                                            prefs_.prefs(), &event_router_));
  }

  void ResetIdentityAPI(std::unique_ptr<IdentityAPI> new_api) {
    api_ = std::move(new_api);
  }

  content::BrowserTaskEnvironment* task_env() { return &task_env_; }

  signin::IdentityTestEnvironment* identity_env() { return &identity_env_; }

  TestExtensionPrefs* prefs() { return &prefs_; }

  IdentityAPI* api() { return api_.get(); }

 private:
  content::BrowserTaskEnvironment task_env_;
  signin::IdentityTestEnvironment identity_env_;
  TestExtensionPrefs prefs_;
  EventRouter event_router_;
  std::unique_ptr<IdentityAPI> api_;
};

// Tests that all accounts in extensions is enabled for regular profiles.
TEST_F(IdentityAPITest, AllAccountsExtensionEnabled) {
  EXPECT_FALSE(api()->AreExtensionsRestrictedToPrimaryAccount());
}

TEST_F(IdentityAPITest, GetGaiaIdForExtension) {
  std::string extension_id = prefs()->AddExtensionAndReturnId("extension");
  std::string gaia_id =
      identity_env()->MakeAccountAvailable("test@example.com").gaia;
  api()->SetGaiaIdForExtension(extension_id, gaia_id);
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension_id), gaia_id);

  std::string another_extension_id =
      prefs()->AddExtensionAndReturnId("another_extension");
  EXPECT_EQ(api()->GetGaiaIdForExtension(another_extension_id), absl::nullopt);
}

TEST_F(IdentityAPITest, GetGaiaIdForExtension_SurvivesShutdown) {
  std::string extension_id = prefs()->AddExtensionAndReturnId("extension");
  std::string gaia_id =
      identity_env()->MakeAccountAvailable("test@example.com").gaia;
  api()->SetGaiaIdForExtension(extension_id, gaia_id);
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension_id), gaia_id);

  api()->Shutdown();
  ResetIdentityAPI(CreateIdentityAPI());
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension_id), gaia_id);
}

TEST_F(IdentityAPITest, EraseGaiaIdForExtension) {
  std::string extension_id = prefs()->AddExtensionAndReturnId("extension");
  CoreAccountInfo account =
      identity_env()->MakeAccountAvailable("test@example.com");
  api()->SetGaiaIdForExtension(extension_id, account.gaia);
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension_id), account.gaia);

  api()->EraseGaiaIdForExtension(extension_id);
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension_id), absl::nullopt);
}

TEST_F(IdentityAPITest, GaiaIdErasedAfterSignOut) {
  std::string extension_id = prefs()->AddExtensionAndReturnId("extension");
  CoreAccountInfo account =
      identity_env()->MakeAccountAvailable("test@example.com");
  api()->SetGaiaIdForExtension(extension_id, account.gaia);
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension_id), account.gaia);

  identity_env()->RemoveRefreshTokenForAccount(account.account_id);
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension_id), absl::nullopt);
}

TEST_F(IdentityAPITest, GaiaIdErasedAfterSignOut_TwoAccounts) {
  std::string extension1_id = prefs()->AddExtensionAndReturnId("extension1");
  CoreAccountInfo account1 =
      identity_env()->MakeAccountAvailable("test1@example.com");
  api()->SetGaiaIdForExtension(extension1_id, account1.gaia);
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension1_id), account1.gaia);

  std::string extension2_id = prefs()->AddExtensionAndReturnId("extension2");
  CoreAccountInfo account2 =
      identity_env()->MakeAccountAvailable("test2@example.com");
  api()->SetGaiaIdForExtension(extension2_id, account2.gaia);
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension2_id), account2.gaia);

  identity_env()->RemoveRefreshTokenForAccount(account1.account_id);
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension1_id), absl::nullopt);
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension2_id), account2.gaia);
}

TEST_F(IdentityAPITest, GaiaIdErasedAfterSignOut_AfterShutdown) {
  std::string extension_id = prefs()->AddExtensionAndReturnId("extension");
  CoreAccountInfo account =
      identity_env()->MakeAccountAvailable("test@example.com");
  api()->SetGaiaIdForExtension(extension_id, account.gaia);
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension_id), account.gaia);

  api()->Shutdown();
  ResetIdentityAPI(nullptr);

  identity_env()->RemoveRefreshTokenForAccount(account.account_id);
  ResetIdentityAPI(CreateIdentityAPI());
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension_id), absl::nullopt);
}

}  // namespace extensions
