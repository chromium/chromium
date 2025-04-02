// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_empty_backend.h"

#include <variant>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/password_manager/android/password_store_android_account_backend.h"
#include "chrome/browser/password_manager/android/password_store_android_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

constexpr char kTestUrl[] = "https://example.com";

}  // namespace

class PasswordStoreEmptyBackendTest : public testing::Test {
 public:
  PasswordStoreEmptyBackendTest() = default;
  ~PasswordStoreEmptyBackendTest() override = default;

 protected:
  PasswordStoreBackend* CreateAndInitBackend() {
    backend_ = std::make_unique<PasswordStoreEmptyBackend>();
    backend_->InitBackend(nullptr, base::DoNothing(), base::DoNothing(),
                          base::DoNothing());
    return backend_.get();
  }

  base::test::SingleThreadTaskEnvironment task_env_;
  std::unique_ptr<PasswordStoreBackend> backend_;
};

TEST_F(PasswordStoreEmptyBackendTest, InitAndShutdownSignalBack) {
  std::unique_ptr<PasswordStoreBackend> backend =
      std::make_unique<PasswordStoreEmptyBackend>();
  base::MockOnceCallback<void(bool)> mock_completion_cb;
  EXPECT_CALL(mock_completion_cb, Run(true));
  backend->InitBackend(nullptr, base::DoNothing(), base::DoNothing(),
                       mock_completion_cb.Get());

  base::MockOnceClosure mock_shutdown_cb;
  EXPECT_CALL(mock_shutdown_cb, Run);
  backend->Shutdown(mock_shutdown_cb.Get());
}

TEST_F(PasswordStoreEmptyBackendTest, NotAbleToSavePasswords) {
  PasswordStoreBackend* backend = CreateAndInitBackend();
  EXPECT_FALSE(backend->IsAbleToSavePasswords());
}

TEST_F(PasswordStoreEmptyBackendTest, GetAllLoginsAsyncReturnsEmpty) {
  PasswordStoreBackend* backend = CreateAndInitBackend();
  base::test::TestFuture<LoginsResultOrError> future;
  backend->GetAllLoginsAsync(future.GetCallback());
  const LoginsResultOrError& result = future.Get();
  EXPECT_TRUE(std::get<LoginsResult>(result).empty());
}

TEST_F(PasswordStoreEmptyBackendTest,
       GetAllLoginsWithAffiliationAndBrandingAsyncReturnsEmpty) {
  PasswordStoreBackend* backend = CreateAndInitBackend();
  base::test::TestFuture<LoginsResultOrError> future;
  backend->GetAllLoginsWithAffiliationAndBrandingAsync(future.GetCallback());
  const LoginsResultOrError& result = future.Get();
  EXPECT_TRUE(std::get<LoginsResult>(result).empty());
}

TEST_F(PasswordStoreEmptyBackendTest, FillMatchingLoginsAsyncReturnsEmpty) {
  PasswordStoreBackend* backend = CreateAndInitBackend();
  base::test::TestFuture<LoginsResultOrError> future;
  std::vector<PasswordFormDigest> forms = {PasswordFormDigest(
      PasswordForm::Scheme::kHtml, kTestUrl, GURL(kTestUrl))};
  backend->FillMatchingLoginsAsync(future.GetCallback(), /*include_psl=*/false,
                                   forms);
  const LoginsResultOrError& result = future.Get();
  EXPECT_TRUE(std::get<LoginsResult>(result).empty());
}

TEST_F(PasswordStoreEmptyBackendTest,
       GetGroupedMatchingLoginsAsyncReturnsEmpty) {
  PasswordStoreBackend* backend = CreateAndInitBackend();
  base::test::TestFuture<LoginsResultOrError> future;
  PasswordFormDigest form_digest(PasswordForm::Scheme::kHtml, kTestUrl,
                                 GURL(kTestUrl));
  backend->GetGroupedMatchingLoginsAsync(form_digest, future.GetCallback());
  const LoginsResultOrError& result = future.Get();
  EXPECT_TRUE(std::get<LoginsResult>(result).empty());
}

TEST_F(PasswordStoreEmptyBackendTest,
       RemoveLoginsCreatedBetweenAsyncReturnsEmpty) {
  PasswordStoreBackend* backend = CreateAndInitBackend();

  base::Time delete_begin = base::Time::FromTimeT(1000);
  base::Time delete_end = base::Time::FromTimeT(2000);
  base::test::TestFuture<PasswordChangesOrError> future;
  backend->RemoveLoginsCreatedBetweenAsync(FROM_HERE, delete_begin, delete_end,
                                           base::DoNothing(),
                                           future.GetCallback());

  const PasswordChangesOrError& result = future.Get();
  EXPECT_TRUE(std::get<PasswordChanges>(result).value().empty());
}

TEST_F(PasswordStoreEmptyBackendTest,
       DisableAutoSignInForOriginsRunsCompletion) {
  PasswordStoreBackend* backend = CreateAndInitBackend();
  base::test::TestFuture<void> future;
  backend->DisableAutoSignInForOriginsAsync(base::NullCallback(),
                                            future.GetCallback());
  EXPECT_TRUE(future.Wait());
}
}  // namespace password_manager
