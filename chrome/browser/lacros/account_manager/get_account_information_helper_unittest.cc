// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/account_manager/get_account_information_helper.h"

#include <vector>

#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/lacros/account_manager/account_profile_mapper.h"
#include "chrome/browser/lacros/identity_manager_lacros.h"
#include "chromeos/crosapi/mojom/identity_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

const char kAccount1Gaia[] = "A";
const char kAccount1FullName[] = "FullName 1";
const char kAccount1Email[] = "email1@gmail.com";

const char kAccount2Gaia[] = "B";
const char kAccount2FullName[] = "FullName 2";
const char kAccount2Email[] = "email2@gmail.com";

const char kNonExistingAccountGaia[] = "Z";

using MockGetAccountInformationCallback = base::MockOnceCallback<void(
    std::vector<GetAccountInformationHelper::GetAccountInformationResult>
        account_information)>;

class MockIdentityManagerLacros : public IdentityManagerLacros {
 public:
  MockIdentityManagerLacros() = default;
  ~MockIdentityManagerLacros() override = default;

  MOCK_METHOD(
      void,
      GetAccountFullName,
      (const std::string& gaia_id,
       crosapi::mojom::IdentityManager::GetAccountFullNameCallback callback));

  MOCK_METHOD(
      void,
      GetAccountImage,
      (const std::string& gaia_id,
       crosapi::mojom::IdentityManager::GetAccountImageCallback callback));

  MOCK_METHOD(
      void,
      GetAccountEmail,
      (const std::string& gaia_id,
       crosapi::mojom::IdentityManager::GetAccountEmailCallback callback));
};

MATCHER_P(
    AccountInformationEqual,
    other,
    "std::vector<GetAccountInformationHelper::GetAccountInformationResult> "
    "equality matcher") {
  if (arg.size() != other.size())
    return false;
  for (unsigned long i = 0; i < arg.size(); i++) {
    if (arg[0].full_name != other[0].full_name ||
        arg[0].email != other[0].email || arg[0].gaia != other[0].gaia) {
      return false;
    }
  }
  return true;
}

}  // namespace

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

class GetAccountInformationHelperTest : public testing::Test {
 public:
  GetAccountInformationHelperTest() {}

  void RunTest(
      std::vector<GetAccountInformationHelper::GetAccountInformationResult>
          expected_accounts) {
    std::unique_ptr<MockIdentityManagerLacros> mock_identity_manager =
        std::make_unique<NiceMock<MockIdentityManagerLacros>>();

    ON_CALL(*mock_identity_manager, GetAccountFullName(_, _))
        .WillByDefault(Invoke(
            [](const std::string& gaia_id,
               crosapi::mojom::IdentityManager::GetAccountFullNameCallback
                   callback) {
              if (gaia_id == kAccount1Gaia) {
                std::move(callback).Run(kAccount1FullName);
                return;
              }
              if (gaia_id == kAccount2Gaia) {
                std::move(callback).Run(kAccount2FullName);
                return;
              }
              std::move(callback).Run("");
            }));
    ON_CALL(*mock_identity_manager, GetAccountEmail(_, _))
        .WillByDefault(
            Invoke([](const std::string& gaia_id,
                      crosapi::mojom::IdentityManager::GetAccountEmailCallback
                          callback) {
              if (gaia_id == kAccount1Gaia) {
                std::move(callback).Run(kAccount1Email);
                return;
              }
              if (gaia_id == kAccount2Gaia) {
                std::move(callback).Run(kAccount2Email);
                return;
              }
              std::move(callback).Run("");
            }));
    ON_CALL(*mock_identity_manager, GetAccountImage(_, _))
        .WillByDefault(Invoke(
            [](const std::string& gaia_id,
               crosapi::mojom::IdentityManager::GetAccountImageCallback
                   callback) { std::move(callback).Run(gfx::ImageSkia()); }));

    std::vector<std::string> gaia_ids;
    for (GetAccountInformationHelper::GetAccountInformationResult
             account_information : expected_accounts) {
      gaia_ids.emplace_back(account_information.gaia);
    }

    MockGetAccountInformationCallback get_information_callback;
    EXPECT_CALL(get_information_callback,
                Run(AccountInformationEqual(expected_accounts)));

    GetAccountInformationHelper get_account_information_helper(
        std::move(mock_identity_manager));
    get_account_information_helper.Start(gaia_ids,
                                         get_information_callback.Get());
  }
};

TEST_F(GetAccountInformationHelperTest, OneAccount) {
  GetAccountInformationHelper::GetAccountInformationResult account1;
  account1.gaia = kAccount1Gaia;
  account1.email = kAccount1Email;
  account1.full_name = kAccount1FullName;

  RunTest({account1});
}

TEST_F(GetAccountInformationHelperTest, MultipleAccounts) {
  GetAccountInformationHelper::GetAccountInformationResult account1;
  account1.gaia = kAccount1Gaia;
  account1.email = kAccount1Email;
  account1.full_name = kAccount1FullName;

  GetAccountInformationHelper::GetAccountInformationResult account2;
  account2.gaia = kAccount2Gaia;
  account2.email = kAccount2Email;
  account2.full_name = kAccount2FullName;

  RunTest({account1, account2});
}

TEST_F(GetAccountInformationHelperTest, NonExistingAccount) {
  GetAccountInformationHelper::GetAccountInformationResult account;
  account.gaia = kNonExistingAccountGaia;
  account.email = "";
  account.full_name = "";

  RunTest({account});
}

TEST_F(GetAccountInformationHelperTest, NoAccounts) {
  RunTest({});
}
