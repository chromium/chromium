// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_notification/push_notification_service_desktop_impl.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kPushNotificationAppId[] = "com.google.chrome.push_notification";
const char kSenderIdFCMToken[] = "sharing_fcm_token";
const char kSharingSenderID[] = "745476177629";

namespace {

class FakeInstanceID : public instance_id::InstanceID {
 public:
  explicit FakeInstanceID(gcm::FakeGCMDriver* gcm_driver)
      : InstanceID(kPushNotificationAppId, gcm_driver) {}
  ~FakeInstanceID() override = default;

  void GetID(GetIDCallback callback) override { NOTIMPLEMENTED(); }

  void GetCreationTime(GetCreationTimeCallback callback) override {
    NOTIMPLEMENTED();
  }

  void GetToken(const std::string& authorized_entity,
                const std::string& scope,
                base::TimeDelta time_to_live,
                std::set<Flags> flags,
                GetTokenCallback callback) override {
    if (authorized_entity == kSharingSenderID) {
      std::move(callback).Run(kSenderIdFCMToken, result_);
    } else {
      std::move(callback).Run(fcm_token_, result_);
    }
  }

  void ValidateToken(const std::string& authorized_entity,
                     const std::string& scope,
                     const std::string& token,
                     ValidateTokenCallback callback) override {
    NOTIMPLEMENTED();
  }

  void DeleteToken(const std::string& authorized_entity,
                   const std::string& scope,
                   DeleteTokenCallback callback) override {
    std::move(callback).Run(result_);
  }

  void DeleteTokenImpl(const std::string& authorized_entity,
                       const std::string& scope,
                       DeleteTokenCallback callback) override {
    NOTIMPLEMENTED();
  }

  void DeleteIDImpl(DeleteIDCallback callback) override { NOTIMPLEMENTED(); }

  void SetFCMResult(InstanceID::Result result) { result_ = result; }

  void SetFCMToken(std::string fcm_token) { fcm_token_ = std::move(fcm_token); }

 private:
  InstanceID::Result result_;
  std::string fcm_token_;
};

class FakeInstanceIDDriver : public instance_id::InstanceIDDriver {
 public:
  FakeInstanceIDDriver() : InstanceIDDriver(nullptr) {}

  FakeInstanceIDDriver(const FakeInstanceIDDriver&) = delete;
  FakeInstanceIDDriver& operator=(const FakeInstanceIDDriver&) = delete;

  ~FakeInstanceIDDriver() override = default;

  instance_id::InstanceID* GetInstanceID(const std::string& app_id) override {
    return fake_instance_id_.get();
  }

  void SetFakeInstanceID(std::unique_ptr<FakeInstanceID> fake_instance_id) {
    fake_instance_id_ = std::move(fake_instance_id);
  }

 private:
  std::unique_ptr<FakeInstanceID> fake_instance_id_;
};

}  // namespace

namespace push_notification {

class PushNotificationServiceDesktopImplTest : public testing::Test {
 public:
  PushNotificationServiceDesktopImplTest() {
    shared_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            base::BindOnce([]() -> network::mojom::URLLoaderFactory* {
              ADD_FAILURE() << "Did not expect this to actually be used";
              return nullptr;
            }));
  }

  ~PushNotificationServiceDesktopImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    fake_gcm_driver_ = std::make_unique<gcm::FakeGCMDriver>();
    fake_instance_id_ =
        std::make_unique<FakeInstanceID>(fake_gcm_driver_.get());
    fake_instance_id_driver_.SetFakeInstanceID(std::move(fake_instance_id_));
    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>(
        &test_url_loader_factory_, nullptr, nullptr);
    push_notification_service_ =
        std::make_unique<PushNotificationServiceDesktopImpl>(
            pref_service_.get(), &fake_instance_id_driver_,
            identity_test_env_->identity_manager(),
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_));
  }

  void TearDown() override {
    push_notification_service_.reset();
    identity_test_env_.reset();
    pref_service_.reset();
    fake_gcm_driver_.reset();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<PushNotificationServiceDesktopImpl>
      push_notification_service_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  FakeInstanceIDDriver fake_instance_id_driver_;
  std::unique_ptr<FakeInstanceID> fake_instance_id_;
  std::unique_ptr<gcm::FakeGCMDriver> fake_gcm_driver_;
};

TEST_F(PushNotificationServiceDesktopImplTest, StartService) {
  EXPECT_TRUE(push_notification_service_);
}

}  // namespace push_notification
