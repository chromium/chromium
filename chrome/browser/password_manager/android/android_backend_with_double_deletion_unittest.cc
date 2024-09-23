// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/android_backend_with_double_deletion.h"

#include "base/task/sequenced_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_backend.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::NiceMock;
using ::testing::Return;

PasswordForm CreateTestForm() {
  PasswordForm form;
  form.username_value = u"admin";
  form.password_value = u"admin";
  form.signon_realm = "https://admin.google.com/";
  form.url = GURL(form.signon_realm);
  return form;
}

}  // namespace

class AndroidBackendWithDoubleDeletionTest : public testing::Test {
 protected:
  AndroidBackendWithDoubleDeletionTest() {
    auto built_in_backend =
        std::make_unique<NiceMock<MockPasswordStoreBackend>>();
    auto android_backend =
        std::make_unique<NiceMock<MockPasswordStoreBackend>>();
    built_in_backend_ = built_in_backend.get();
    android_backend_ = android_backend.get();
    proxy_backend_ = std::make_unique<AndroidBackendWithDoubleDeletion>(
        std::move(built_in_backend), std::move(android_backend));

    EXPECT_CALL(*built_in_backend_, InitBackend);
    EXPECT_CALL(*android_backend_, InitBackend);
    proxy_backend()->InitBackend(nullptr, base::DoNothing(), base::DoNothing(),
                                 base::DoNothing());
  }

  ~AndroidBackendWithDoubleDeletionTest() override {
    EXPECT_CALL(android_backend(), Shutdown);
    EXPECT_CALL(built_in_backend(), Shutdown);
    built_in_backend_ = nullptr;
    android_backend_ = nullptr;
    proxy_backend()->Shutdown(base::DoNothing());
  }

  PasswordStoreBackend* proxy_backend() { return proxy_backend_.get(); }
  MockPasswordStoreBackend& built_in_backend() { return *built_in_backend_; }
  MockPasswordStoreBackend& android_backend() { return *android_backend_; }

 private:
  base::test::SingleThreadTaskEnvironment task_env_;
  raw_ptr<NiceMock<MockPasswordStoreBackend>> built_in_backend_;
  raw_ptr<NiceMock<MockPasswordStoreBackend>> android_backend_;
  std::unique_ptr<AndroidBackendWithDoubleDeletion> proxy_backend_;
};

TEST_F(AndroidBackendWithDoubleDeletionTest, GetAllLoginsAsync) {
  EXPECT_CALL(built_in_backend(), GetAllLoginsAsync).Times(0);
  EXPECT_CALL(android_backend(), GetAllLoginsAsync);

  proxy_backend()->GetAllLoginsAsync(base::DoNothing());
}

TEST_F(AndroidBackendWithDoubleDeletionTest, IsAbleToSavePasswords) {
  EXPECT_CALL(built_in_backend(), IsAbleToSavePasswords).Times(0);
  EXPECT_CALL(android_backend(), IsAbleToSavePasswords).WillOnce(Return(false));

  EXPECT_FALSE(proxy_backend()->IsAbleToSavePasswords());
}

TEST_F(AndroidBackendWithDoubleDeletionTest,
       GetAllLoginsWithAffiliationAndBrandingAsync) {
  EXPECT_CALL(built_in_backend(), GetAllLoginsWithAffiliationAndBrandingAsync)
      .Times(0);
  EXPECT_CALL(android_backend(), GetAllLoginsWithAffiliationAndBrandingAsync);

  proxy_backend()->GetAllLoginsWithAffiliationAndBrandingAsync(
      base::DoNothing());
}

TEST_F(AndroidBackendWithDoubleDeletionTest, GetAutofillableLoginsAsync) {
  EXPECT_CALL(built_in_backend(), GetAutofillableLoginsAsync).Times(0);
  EXPECT_CALL(android_backend(), GetAutofillableLoginsAsync);

  proxy_backend()->GetAutofillableLoginsAsync(base::DoNothing());
}

TEST_F(AndroidBackendWithDoubleDeletionTest, FillMatchingLoginsAsync) {
  std::vector<PasswordFormDigest> forms = {
      PasswordFormDigest(PasswordForm::Scheme::kHtml, "https://google.com/",
                         GURL("https://google.com/"))};
  EXPECT_CALL(built_in_backend(), FillMatchingLoginsAsync).Times(0);
  EXPECT_CALL(android_backend(),
              FillMatchingLoginsAsync(_, false, ElementsAreArray(forms)));

  proxy_backend()->FillMatchingLoginsAsync(base::DoNothing(),
                                           /*include_psl=*/false, forms);
}

TEST_F(AndroidBackendWithDoubleDeletionTest, GetGroupedMatchingLoginsAsync) {
  PasswordFormDigest form_digest(PasswordForm::Scheme::kHtml,
                                 "https://google.com/",
                                 GURL("https://google.com/"));
  EXPECT_CALL(built_in_backend(), GetGroupedMatchingLoginsAsync).Times(0);
  EXPECT_CALL(android_backend(), GetGroupedMatchingLoginsAsync(form_digest, _));

  proxy_backend()->GetGroupedMatchingLoginsAsync(form_digest,
                                                 base::DoNothing());
}

TEST_F(AndroidBackendWithDoubleDeletionTest, AddLoginAsync) {
  EXPECT_CALL(built_in_backend(), AddLoginAsync).Times(0);
  EXPECT_CALL(android_backend(), AddLoginAsync(CreateTestForm(), _));

  proxy_backend()->AddLoginAsync(CreateTestForm(), base::DoNothing());
}

TEST_F(AndroidBackendWithDoubleDeletionTest, UpdateLoginAsync) {
  EXPECT_CALL(built_in_backend(), UpdateLoginAsync).Times(0);
  EXPECT_CALL(android_backend(), UpdateLoginAsync(CreateTestForm(), _));

  proxy_backend()->UpdateLoginAsync(CreateTestForm(), base::DoNothing());
}

TEST_F(AndroidBackendWithDoubleDeletionTest, RemoveLoginAsync) {
  EXPECT_CALL(built_in_backend(), RemoveLoginAsync(_, CreateTestForm(), _));
  EXPECT_CALL(android_backend(), RemoveLoginAsync(_, CreateTestForm(), _));

  proxy_backend()->RemoveLoginAsync(FROM_HERE, CreateTestForm(),
                                    base::DoNothing());
}

TEST_F(AndroidBackendWithDoubleDeletionTest, RemoveLoginsByURLAndTimeAsync) {
  base::RepeatingCallback<bool(const GURL&)> url_filter =
      base::BindRepeating([](const GURL& url) { return true; });
  base::Time delete_begin = base::Time::FromTimeT(1000);
  base::Time delete_end = base::Time::FromTimeT(2000);
  EXPECT_CALL(built_in_backend(),
              RemoveLoginsByURLAndTimeAsync(_, url_filter, delete_begin,
                                            delete_end, _, _));
  EXPECT_CALL(android_backend(),
              RemoveLoginsByURLAndTimeAsync(_, url_filter, delete_begin,
                                            delete_end, _, _));

  proxy_backend()->RemoveLoginsByURLAndTimeAsync(
      FROM_HERE, url_filter, delete_begin, delete_end,
      base::OnceCallback<void(bool)>(), base::DoNothing());
}

TEST_F(AndroidBackendWithDoubleDeletionTest, RemoveLoginsCreatedBetweenAsync) {
  base::Time delete_begin = base::Time::FromTimeT(1000);
  base::Time delete_end = base::Time::FromTimeT(2000);
  EXPECT_CALL(built_in_backend(),
              RemoveLoginsCreatedBetweenAsync(_, delete_begin, delete_end, _));
  EXPECT_CALL(android_backend(),
              RemoveLoginsCreatedBetweenAsync(_, delete_begin, delete_end, _));

  proxy_backend()->RemoveLoginsCreatedBetweenAsync(
      FROM_HERE, delete_begin, delete_end, base::DoNothing());
}

TEST_F(AndroidBackendWithDoubleDeletionTest, DisableAutoSignInForOriginsAsync) {
  base::RepeatingCallback<bool(const GURL&)> url_filter =
      base::BindRepeating([](const GURL& url) { return true; });

  EXPECT_CALL(built_in_backend(), DisableAutoSignInForOriginsAsync).Times(0);
  EXPECT_CALL(android_backend(),
              DisableAutoSignInForOriginsAsync(url_filter, _));

  proxy_backend()->DisableAutoSignInForOriginsAsync(url_filter,
                                                    base::DoNothing());
}

}  // namespace password_manager
