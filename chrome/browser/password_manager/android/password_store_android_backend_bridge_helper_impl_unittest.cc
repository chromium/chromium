// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_backend_bridge_helper_impl.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_bridge.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_bridge_helper.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_consumer_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::NiceMock;
using testing::Optional;
using testing::Return;
using testing::StrictMock;
using testing::VariantWith;
using testing::WithArg;
using JobId = PasswordStoreAndroidBackendBridge::JobId;

constexpr char kTestAccount[] = "test@gmail.com";
const std::u16string kTestUsername(u"Todd Tester");
const std::u16string kTestPassword(u"S3cr3t");
constexpr char kTestUrl[] = "https://example.com";
constexpr base::Time kTestDateCreated = base::Time::FromTimeT(1500);

PasswordForm CreateTestLogin() {
  PasswordForm form;
  form.username_value = kTestUsername;
  form.password_value = kTestPassword;
  form.url = GURL(kTestUrl);
  form.signon_realm = kTestUrl;
  form.date_created = kTestDateCreated;
  return form;
}

MATCHER_P(ExpectSyncingAccount, expectation, "") {
  return absl::holds_alternative<
             PasswordStoreAndroidBackendBridge::SyncingAccount>(arg) &&
         expectation ==
             absl::get<PasswordStoreAndroidBackendBridge::SyncingAccount>(arg)
                 .value();
}

class MockBackendConsumer
    : public PasswordStoreAndroidBackendConsumerBridge::Consumer {
  MOCK_METHOD(void,
              OnCompleteWithLogins,
              (JobId, std::vector<PasswordForm>),
              (override));
  MOCK_METHOD(void, OnLoginsChanged, (JobId, PasswordChanges), (override));
  MOCK_METHOD(void, OnError, (JobId, AndroidBackendError), (override));
};

class MockPasswordStoreAndroidBackendConsumerBridge
    : public PasswordStoreAndroidBackendConsumerBridge {
 public:
  MOCK_METHOD(void, SetConsumer, (base::WeakPtr<Consumer>), (override));
  MOCK_METHOD(base::android::ScopedJavaGlobalRef<jobject>,
              GetJavaBridge,
              (),
              (const, override));
};

class MockPasswordStoreAndroidBackendBridge
    : public PasswordStoreAndroidBackendBridge {
 public:
  MOCK_METHOD(void,
              Init,
              (const PasswordStoreAndroidBackendConsumerBridge&),
              (override));
  MOCK_METHOD(void, GetAllLogins, (JobId, Account), (override));
  MOCK_METHOD(void, GetAutofillableLogins, (JobId, Account), (override));
  MOCK_METHOD(void,
              GetLoginsForSignonRealm,
              (JobId, const std::string&, Account),
              (override));
  MOCK_METHOD(void,
              AddLogin,
              (JobId, const PasswordForm&, Account),
              (override));
  MOCK_METHOD(void,
              UpdateLogin,
              (JobId, const PasswordForm&, Account),
              (override));
  MOCK_METHOD(void,
              RemoveLogin,
              (JobId, const PasswordForm&, Account),
              (override));
  MOCK_METHOD(void, ShowErrorNotification, (), (override));
};

}  // namespace

class PasswordStoreAndroidBackendBridgeHelperImplTest : public testing::Test {
 protected:
  PasswordStoreAndroidBackendBridgeHelperImplTest()
      : helper_(base::PassKey<
                    class PasswordStoreAndroidBackendBridgeHelperImplTest>(),
                CreateMockConsumerBridge(),
                CreateMockBridge()) {
    EXPECT_CALL(*bridge(), Init);
    EXPECT_CALL(*consumer_bridge(), SetConsumer);
    helper_.SetConsumer(consumer_weak_factory_.GetWeakPtr());
    RunUntilIdle();
  }

  ~PasswordStoreAndroidBackendBridgeHelperImplTest() override {
    RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(consumer_bridge_);
    testing::Mock::VerifyAndClearExpectations(bridge_);
    testing::Mock::VerifyAndClearExpectations(&consumer_);
  }

  MockPasswordStoreAndroidBackendConsumerBridge* consumer_bridge() {
    return consumer_bridge_;
  }
  MockPasswordStoreAndroidBackendBridge* bridge() { return bridge_; }
  PasswordStoreAndroidBackendBridgeHelperImpl* helper() { return &helper_; }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};

 private:
  std::unique_ptr<MockPasswordStoreAndroidBackendConsumerBridge>
  CreateMockConsumerBridge() {
    auto unique_consumer_bridge = std::make_unique<
        StrictMock<MockPasswordStoreAndroidBackendConsumerBridge>>();
    consumer_bridge_ = unique_consumer_bridge.get();

    return unique_consumer_bridge;
  }

  std::unique_ptr<PasswordStoreAndroidBackendBridge> CreateMockBridge() {
    auto unique_bridge =
        std::make_unique<StrictMock<MockPasswordStoreAndroidBackendBridge>>();
    bridge_ = unique_bridge.get();
    return unique_bridge;
  }

  raw_ptr<StrictMock<MockPasswordStoreAndroidBackendConsumerBridge>>
      consumer_bridge_;
  raw_ptr<StrictMock<MockPasswordStoreAndroidBackendBridge>> bridge_;
  PasswordStoreAndroidBackendBridgeHelperImpl helper_;
  StrictMock<MockBackendConsumer> consumer_;
  base::WeakPtrFactory<MockBackendConsumer> consumer_weak_factory_{&consumer_};
};

TEST_F(PasswordStoreAndroidBackendBridgeHelperImplTest,
       GetAllLoginsCallsBridge) {
  JobId job_id = helper()->GetAllLogins(
      PasswordStoreAndroidBackendBridge::SyncingAccount(kTestAccount));
  EXPECT_CALL(*bridge(),
              GetAllLogins(job_id, ExpectSyncingAccount(kTestAccount)));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendBridgeHelperImplTest,
       GetAutofillableLoginsCallsBridge) {
  JobId job_id = helper()->GetAutofillableLogins(
      PasswordStoreAndroidBackendBridge::SyncingAccount(kTestAccount));
  EXPECT_CALL(*bridge(), GetAutofillableLogins(
                             job_id, ExpectSyncingAccount(kTestAccount)));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendBridgeHelperImplTest,
       GetLoginsForSignonRealmCallsBridge) {
  JobId job_id = helper()->GetLoginsForSignonRealm(
      kTestUrl,
      PasswordStoreAndroidBackendBridge::SyncingAccount(kTestAccount));
  EXPECT_CALL(*bridge(),
              GetLoginsForSignonRealm(job_id, kTestUrl,
                                      ExpectSyncingAccount(kTestAccount)));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendBridgeHelperImplTest, AddLoginCallsBridge) {
  auto form = CreateTestLogin();
  JobId job_id = helper()->AddLogin(
      form, PasswordStoreAndroidBackendBridge::SyncingAccount(kTestAccount));
  EXPECT_CALL(*bridge(),
              AddLogin(job_id, Eq(form), ExpectSyncingAccount(kTestAccount)));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendBridgeHelperImplTest,
       UpdateLoginCallsBridge) {
  auto form = CreateTestLogin();
  JobId job_id = helper()->UpdateLogin(
      form, PasswordStoreAndroidBackendBridge::SyncingAccount(kTestAccount));
  EXPECT_CALL(*bridge(), UpdateLogin(job_id, Eq(form),
                                     ExpectSyncingAccount(kTestAccount)));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendBridgeHelperImplTest,
       RemoveLoginCallsBridge) {
  auto form = CreateTestLogin();
  JobId job_id = helper()->RemoveLogin(
      form, PasswordStoreAndroidBackendBridge::SyncingAccount(kTestAccount));
  EXPECT_CALL(*bridge(), RemoveLogin(job_id, Eq(form),
                                     ExpectSyncingAccount(kTestAccount)));
  RunUntilIdle();
}

}  // namespace password_manager
