// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_backend_bridge_helper_impl.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_bridge_helper.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_dispatcher_bridge.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_receiver_bridge.h"
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
using JobId = PasswordStoreAndroidBackendDispatcherBridge::JobId;

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

class MockBackendConsumer
    : public PasswordStoreAndroidBackendReceiverBridge::Consumer {
  MOCK_METHOD(void,
              OnCompleteWithLogins,
              (JobId, std::vector<PasswordForm>),
              (override));
  MOCK_METHOD(void, OnLoginsChanged, (JobId, PasswordChanges), (override));
  MOCK_METHOD(void, OnError, (JobId, AndroidBackendError), (override));
};

class MockPasswordStoreAndroidBackendReceiverBridge
    : public PasswordStoreAndroidBackendReceiverBridge {
 public:
  MOCK_METHOD(void, SetConsumer, (base::WeakPtr<Consumer>), (override));
  MOCK_METHOD(base::android::ScopedJavaGlobalRef<jobject>,
              GetJavaBridge,
              (),
              (const, override));
};

class MockPasswordStoreAndroidBackendDispatcherBridge
    : public PasswordStoreAndroidBackendDispatcherBridge {
 public:
  MOCK_METHOD(void,
              Init,
              (base::android::ScopedJavaGlobalRef<jobject>),
              (override));
  MOCK_METHOD(void, GetAllLogins, (JobId, std::string), (override));
  MOCK_METHOD(void,
              GetAllLoginsWithBrandingInfo,
              (JobId, std::string),
              (override));
  MOCK_METHOD(void, GetAutofillableLogins, (JobId, std::string), (override));
  MOCK_METHOD(void,
              GetLoginsForSignonRealm,
              (JobId, const std::string&, std::string),
              (override));
  MOCK_METHOD(void,
              GetAffiliatedLoginsForSignonRealm,
              (JobId, const std::string&, std::string),
              (override));
  MOCK_METHOD(void,
              AddLogin,
              (JobId, const PasswordForm&, std::string),
              (override));
  MOCK_METHOD(void,
              UpdateLogin,
              (JobId, const PasswordForm&, std::string),
              (override));
  MOCK_METHOD(void,
              RemoveLogin,
              (JobId, const PasswordForm&, std::string),
              (override));
};

}  // namespace

class PasswordStoreAndroidBackendBridgeHelperImplTest : public testing::Test {
 protected:
  PasswordStoreAndroidBackendBridgeHelperImplTest()
      : helper_(base::PassKey<
                    class PasswordStoreAndroidBackendBridgeHelperImplTest>(),
                CreateMockReceiverBridge(),
                CreateMockDispatcherBridge()) {
    helper_.SetConsumer(consumer_weak_factory_.GetWeakPtr());
    RunUntilIdle();
  }

  ~PasswordStoreAndroidBackendBridgeHelperImplTest() override {
    RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(receiver_bridge_);
    testing::Mock::VerifyAndClearExpectations(dispatcher_bridge_);
    testing::Mock::VerifyAndClearExpectations(&consumer_);
  }

  MockPasswordStoreAndroidBackendReceiverBridge* receiver_bridge() {
    return receiver_bridge_;
  }
  MockPasswordStoreAndroidBackendDispatcherBridge* dispatcher_bridge() {
    return dispatcher_bridge_;
  }
  PasswordStoreAndroidBackendBridgeHelperImpl* helper() { return &helper_; }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};

 private:
  std::unique_ptr<MockPasswordStoreAndroidBackendReceiverBridge>
  CreateMockReceiverBridge() {
    auto unique_receiver_bridge = std::make_unique<
        StrictMock<MockPasswordStoreAndroidBackendReceiverBridge>>();
    receiver_bridge_ = unique_receiver_bridge.get();

    EXPECT_CALL(*receiver_bridge(), GetJavaBridge);
    EXPECT_CALL(*receiver_bridge(), SetConsumer);

    return unique_receiver_bridge;
  }

  std::unique_ptr<PasswordStoreAndroidBackendDispatcherBridge>
  CreateMockDispatcherBridge() {
    auto unique_dispatcher_bridge = std::make_unique<
        StrictMock<MockPasswordStoreAndroidBackendDispatcherBridge>>();
    dispatcher_bridge_ = unique_dispatcher_bridge.get();

    EXPECT_CALL(*dispatcher_bridge(), Init);

    return unique_dispatcher_bridge;
  }

  raw_ptr<StrictMock<MockPasswordStoreAndroidBackendReceiverBridge>>
      receiver_bridge_;
  raw_ptr<StrictMock<MockPasswordStoreAndroidBackendDispatcherBridge>>
      dispatcher_bridge_;
  PasswordStoreAndroidBackendBridgeHelperImpl helper_;
  StrictMock<MockBackendConsumer> consumer_;
  base::WeakPtrFactory<MockBackendConsumer> consumer_weak_factory_{&consumer_};
};

TEST_F(PasswordStoreAndroidBackendBridgeHelperImplTest,
       GetAllLoginsCallsBridge) {
  JobId job_id = helper()->GetAllLogins(kTestAccount);
  EXPECT_CALL(*dispatcher_bridge(), GetAllLogins(job_id, kTestAccount));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendBridgeHelperImplTest,
       GetAutofillableLoginsCallsBridge) {
  JobId job_id = helper()->GetAutofillableLogins(kTestAccount);
  EXPECT_CALL(*dispatcher_bridge(),
              GetAutofillableLogins(job_id, kTestAccount));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendBridgeHelperImplTest,
       GetLoginsForSignonRealmCallsBridge) {
  JobId job_id = helper()->GetLoginsForSignonRealm(kTestUrl, kTestAccount);
  EXPECT_CALL(*dispatcher_bridge(),
              GetLoginsForSignonRealm(job_id, kTestUrl, kTestAccount));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendBridgeHelperImplTest, AddLoginCallsBridge) {
  auto form = CreateTestLogin();
  JobId job_id = helper()->AddLogin(form, kTestAccount);
  EXPECT_CALL(*dispatcher_bridge(), AddLogin(job_id, Eq(form), kTestAccount));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendBridgeHelperImplTest,
       UpdateLoginCallsBridge) {
  auto form = CreateTestLogin();
  JobId job_id = helper()->UpdateLogin(form, kTestAccount);
  EXPECT_CALL(*dispatcher_bridge(),
              UpdateLogin(job_id, Eq(form), kTestAccount));
  RunUntilIdle();
}

TEST_F(PasswordStoreAndroidBackendBridgeHelperImplTest,
       RemoveLoginCallsBridge) {
  auto form = CreateTestLogin();
  JobId job_id = helper()->RemoveLogin(form, kTestAccount);
  EXPECT_CALL(*dispatcher_bridge(),
              RemoveLogin(job_id, Eq(form), kTestAccount));
  RunUntilIdle();
}

}  // namespace password_manager
