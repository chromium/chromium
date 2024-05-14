// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/android_sms/connection_manager.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/android_sms/android_sms_urls.h"
#include "chrome/browser/ash/android_sms/fake_android_sms_app_manager.h"
#include "chrome/browser/ash/android_sms/fake_connection_establisher.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/fake_service_worker_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace android_sms {

namespace {

const int64_t kDummyVersionId = 123l;
const int64_t kDummyVersionId2 = 456l;

GURL GetAndroidMessagesURLOld() {
  // For this test, consider the staging server to be the "old" URL.
  return GetAndroidMessagesURL(false /* use_install_url */,
                               PwaDomain::kStaging);
}

}  // namespace

class ConnectionManagerTest : public testing::Test {
 protected:
  class TestServiceWorkerProvider
      : public ConnectionManager::ServiceWorkerProvider {
   public:
    TestServiceWorkerProvider(
        content::FakeServiceWorkerContext* new_url_service_worker,
        content::FakeServiceWorkerContext* old_url_service_worker)
        : new_url_service_worker_(new_url_service_worker),
          old_url_service_worker_(old_url_service_worker) {}

    TestServiceWorkerProvider(const TestServiceWorkerProvider&) = delete;
    TestServiceWorkerProvider& operator=(const TestServiceWorkerProvider&) =
        delete;

    ~TestServiceWorkerProvider() override = default;

   private:
    // ConnectionManager::ServiceWorkerProvider:
    content::ServiceWorkerContext* Get(const GURL& url,
                                       Profile* profile) override {
      if (url == GetAndroidMessagesURL())
        return new_url_service_worker_;

      if (url == GetAndroidMessagesURLOld())
        return old_url_service_worker_;

      NOTREACHED_IN_MIGRATION();
      return nullptr;
    }

    raw_ptr<content::FakeServiceWorkerContext> new_url_service_worker_;
    raw_ptr<content::FakeServiceWorkerContext> old_url_service_worker_;
  };

  enum class PwaState { kEnabledWithNewUrl, kEnabledWithOldUrl, kDisabled };

  ConnectionManagerTest() = default;

  ConnectionManagerTest(const ConnectionManagerTest&) = delete;
  ConnectionManagerTest& operator=(const ConnectionManagerTest&) = delete;

  ~ConnectionManagerTest() override = default;

  void SetUp() override {
    fake_new_service_worker_context_ =
        std::make_unique<content::FakeServiceWorkerContext>();
    fake_old_service_worker_context_ =
        std::make_unique<content::FakeServiceWorkerContext>();
    fake_android_sms_app_manager_ =
        std::make_unique<FakeAndroidSmsAppManager>();
    fake_multidevice_setup_client_ =
        std::make_unique<multidevice_setup::FakeMultiDeviceSetupClient>();
  }

  void SetupConnectionManager(PwaState initial_pwa_state) {
    SetPwaState(initial_pwa_state);

    auto fake_connection_establisher =
        std::make_unique<FakeConnectionEstablisher>();
    fake_connection_establisher_ = fake_connection_establisher.get();

    auto test_service_worker_provider =
        std::make_unique<TestServiceWorkerProvider>(
            fake_new_service_worker_context_.get(),
            fake_old_service_worker_context_.get());
    test_service_worker_provider_ = test_service_worker_provider.get();

    connection_manager_ = base::WrapUnique(
        new ConnectionManager(std::move(fake_connection_establisher), &profile_,
                              fake_android_sms_app_manager_.get(),
                              fake_multidevice_setup_client_.get(),
                              std::move(test_service_worker_provider)));
  }

  void VerifyEstablishConnectionCalls(
      size_t expected_count,
      bool is_last_call_expected_to_be_new_url = true) {
    const auto& establish_connection_calls =
        fake_connection_establisher_->establish_connection_calls();
    EXPECT_EQ(expected_count, establish_connection_calls.size());

    if (expected_count == 0u)
      return;

    if (is_last_call_expected_to_be_new_url) {
      EXPECT_EQ(GetAndroidMessagesURL(),
                std::get<0>(establish_connection_calls.back()));
      EXPECT_EQ(fake_new_service_worker_context_.get(),
                std::get<2>(establish_connection_calls.back()));
    } else {
      EXPECT_EQ(GetAndroidMessagesURLOld(),
                std::get<0>(establish_connection_calls.back()));
      EXPECT_EQ(fake_old_service_worker_context_.get(),
                std::get<2>(establish_connection_calls.back()));
    }
  }

  void VerifyTearDownConnectionCalls(
      size_t expected_count,
      bool is_last_call_expected_to_be_new_url = true) {
    const auto& tear_down_connection_calls =
        fake_connection_establisher_->tear_down_connection_calls();
    EXPECT_EQ(expected_count, tear_down_connection_calls.size());

    if (expected_count == 0u)
      return;

    if (is_last_call_expected_to_be_new_url) {
      EXPECT_EQ(GetAndroidMessagesURL(),
                std::get<0>(tear_down_connection_calls.back()));
      EXPECT_EQ(fake_new_service_worker_context_.get(),
                std::get<1>(tear_down_connection_calls.back()));
    } else {
      EXPECT_EQ(GetAndroidMessagesURLOld(),
                std::get<0>(tear_down_connection_calls.back()));
      EXPECT_EQ(fake_old_service_worker_context_.get(),
                std::get<1>(tear_down_connection_calls.back()));
    }
  }

  void SetPwaState(PwaState pwa_state) {
    if (pwa_state == PwaState::kDisabled) {
      fake_android_sms_app_manager_->SetInstalledAppUrl(std::nullopt);
      fake_multidevice_setup_client_->SetFeatureState(
          multidevice_setup::mojom::Feature::kMessages,
          multidevice_setup::mojom::FeatureState::kDisabledByUser);
      return;
    }

    fake_android_sms_app_manager_->SetInstalledAppUrl(
        pwa_state == PwaState::kEnabledWithNewUrl ? GetAndroidMessagesURL()
                                                  : GetAndroidMessagesURLOld());
    fake_multidevice_setup_client_->SetFeatureState(
        multidevice_setup::mojom::Feature::kMessages,
        multidevice_setup::mojom::FeatureState::kEnabledByUser);
  }

  content::FakeServiceWorkerContext* fake_new_service_worker_context() const {
    return fake_new_service_worker_context_.get();
  }

  content::FakeServiceWorkerContext* fake_old_service_worker_context() const {
    return fake_old_service_worker_context_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;
  std::unique_ptr<content::FakeServiceWorkerContext>
      fake_new_service_worker_context_;
  std::unique_ptr<content::FakeServiceWorkerContext>
      fake_old_service_worker_context_;
  std::unique_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
  std::unique_ptr<FakeAndroidSmsAppManager> fake_android_sms_app_manager_;
  raw_ptr<FakeConnectionEstablisher, DanglingUntriaged>
      fake_connection_establisher_;
  raw_ptr<TestServiceWorkerProvider, DanglingUntriaged>
      test_service_worker_provider_;

  std::unique_ptr<ConnectionManager> connection_manager_;
};

TEST_F(ConnectionManagerTest, ConnectOnActivate) {
  SetupConnectionManager(PwaState::kEnabledWithNewUrl);
  fake_new_service_worker_context()->NotifyObserversOnVersionActivated(
      kDummyVersionId, GetAndroidMessagesURL());

  // Verify that connection establishing is attempted on startup and
  // when a new version is activated.
  // startup + activate.
  VerifyEstablishConnectionCalls(2u /* expected_count */);
}

TEST_F(ConnectionManagerTest, ConnectOnNoControllees) {
  SetupConnectionManager(PwaState::kEnabledWithNewUrl);
  // Notify Activation so that Connection manager is tracking the version ID.
  fake_new_service_worker_context()->NotifyObserversOnVersionActivated(
      kDummyVersionId, GetAndroidMessagesURL());

  // Verify that connection establishing is attempted when there are no
  // controllees.
  fake_new_service_worker_context()->NotifyObserversOnNoControllees(
      kDummyVersionId, GetAndroidMessagesURL());
  // startup + activate + no controllees.
  VerifyEstablishConnectionCalls(3u /* expected_count */);
}

TEST_F(ConnectionManagerTest, IgnoreRedundantVersion) {
  SetupConnectionManager(PwaState::kEnabledWithNewUrl);
  fake_new_service_worker_context()->NotifyObserversOnVersionActivated(
      kDummyVersionId, GetAndroidMessagesURL());

  // Notify that current active version is now redundant.
  fake_new_service_worker_context()->NotifyObserversOnVersionRedundant(
      kDummyVersionId, GetAndroidMessagesURL());

  // Verify that no connection establishing is attempted when there are no
  // controllees for the redundant version.
  fake_new_service_worker_context()->NotifyObserversOnNoControllees(
      kDummyVersionId, GetAndroidMessagesURL());
  // startup + activate only.
  VerifyEstablishConnectionCalls(2u /* expected_count */);
}

TEST_F(ConnectionManagerTest, ConnectOnNoControlleesWithNoActive) {
  SetupConnectionManager(PwaState::kEnabledWithNewUrl);
  // Verify that connection establishing is attempted when there are no
  // controllees for a version ID even if the activate notification was missed.
  fake_new_service_worker_context()->NotifyObserversOnNoControllees(
      kDummyVersionId, GetAndroidMessagesURL());
  // startup + no controllees.
  VerifyEstablishConnectionCalls(2u /* expected_count */);
}

TEST_F(ConnectionManagerTest, IgnoreOnNoControlleesInvalidId) {
  SetupConnectionManager(PwaState::kEnabledWithNewUrl);
  fake_new_service_worker_context()->NotifyObserversOnVersionActivated(
      kDummyVersionId, GetAndroidMessagesURL());

  // Verify that OnNoControllees with different version id is ignored.
  fake_new_service_worker_context()->NotifyObserversOnNoControllees(
      kDummyVersionId2, GetAndroidMessagesURL());
  // startup + activate only.
  VerifyEstablishConnectionCalls(2u /* expected_count */);
}

TEST_F(ConnectionManagerTest, InvalidScope) {
  SetupConnectionManager(PwaState::kEnabledWithNewUrl);
  GURL invalid_scope("https://example.com");
  // Verify that OnVersionActivated and OnNoControllees with invalid scope
  // are ignored
  fake_new_service_worker_context()->NotifyObserversOnVersionActivated(
      kDummyVersionId, invalid_scope);
  fake_new_service_worker_context()->NotifyObserversOnNoControllees(
      kDummyVersionId, invalid_scope);
  // startup only, ignore others.
  VerifyEstablishConnectionCalls(1u /* expected_count */);

  // Verify that OnVersionRedundant with invalid scope is ignored
  fake_new_service_worker_context()->NotifyObserversOnVersionActivated(
      kDummyVersionId, GetAndroidMessagesURL());
  fake_new_service_worker_context()->NotifyObserversOnVersionRedundant(
      kDummyVersionId, invalid_scope);
  fake_new_service_worker_context()->NotifyObserversOnNoControllees(
      kDummyVersionId, GetAndroidMessagesURL());
  // startup + activate + no controllees.
  VerifyEstablishConnectionCalls(3u /* expected_count */);
}

TEST_F(ConnectionManagerTest, FeatureStateInitDisabled) {
  // Verify that connection is not established on initialization
  // if the feature is not enabled.
  SetupConnectionManager(PwaState::kDisabled);
  VerifyEstablishConnectionCalls(0u /* expected_count */);

  SetPwaState(PwaState::kEnabledWithNewUrl);
  VerifyEstablishConnectionCalls(1u /* expected_count */);
}

TEST_F(ConnectionManagerTest, FeatureStateChange) {
  SetupConnectionManager(PwaState::kEnabledWithNewUrl);
  fake_new_service_worker_context()->NotifyObserversOnVersionActivated(
      kDummyVersionId, GetAndroidMessagesURL());

  // Verify that disabling feature stops the service worker.
  SetPwaState(PwaState::kDisabled);
  VerifyEstablishConnectionCalls(2u /* expected_count */);
  VerifyTearDownConnectionCalls(1u /* expected_count */);

  // Verify that subsequent service worker events do not trigger connection.
  fake_new_service_worker_context()->NotifyObserversOnNoControllees(
      kDummyVersionId, GetAndroidMessagesURL());
  VerifyEstablishConnectionCalls(2u /* expected_count */);

  // Verify that enabling feature establishes connection again.
  SetPwaState(PwaState::kEnabledWithNewUrl);
  VerifyEstablishConnectionCalls(3u /* expected_count */);

  // Verify that connection is established if the version id changes.
  fake_new_service_worker_context()->NotifyObserversOnNoControllees(
      kDummyVersionId2, GetAndroidMessagesURL());
  VerifyEstablishConnectionCalls(4u /* expected_count */);
}

TEST_F(ConnectionManagerTest, AppUrlMigration) {
  SetupConnectionManager(PwaState::kEnabledWithOldUrl);
  fake_old_service_worker_context()->NotifyObserversOnVersionActivated(
      kDummyVersionId, GetAndroidMessagesURLOld());
  VerifyEstablishConnectionCalls(
      2u /* expected_count */, false /* is_last_call_expected_to_be_new_url */);

  // Switch to the new URL.
  SetPwaState(PwaState::kEnabledWithNewUrl);

  // The ServiceWorker for the old URL should have stopped.
  VerifyTearDownConnectionCalls(
      1u /* expected_count*/, false /* is_last_call_expected_to_be_new_url */);

  // A connection to the new URL should have occurred.
  VerifyEstablishConnectionCalls(
      3u /* expected_count */, true /* is_last_call_expected_to_be_new_url */);

  fake_new_service_worker_context()->NotifyObserversOnVersionActivated(
      kDummyVersionId, GetAndroidMessagesURL());
  VerifyEstablishConnectionCalls(
      4u /* expected_count */, true /* is_last_call_expected_to_be_new_url */);
}

}  // namespace android_sms
}  // namespace ash
