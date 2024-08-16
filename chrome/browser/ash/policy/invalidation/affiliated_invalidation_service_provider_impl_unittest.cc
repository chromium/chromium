// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/invalidation/affiliated_invalidation_service_provider_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_misc_client.h"
#include "components/invalidation/impl/fake_invalidation_handler.h"
#include "components/invalidation/impl/fake_invalidation_service.h"
#include "components/invalidation/impl/fcm_invalidation_service.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/profile_invalidation_provider.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

const char kAffiliatedUserID1[] = "test_1@example.com";
const char kAffiliatedUserID2[] = "test_2@example.com";
const char kUnaffiliatedUserID[] = "test@other_domain.test";

std::variant<std::unique_ptr<invalidation::InvalidationService>,
             std::unique_ptr<invalidation::InvalidationListener>>
CreateInvalidationServiceForSenderId(std::string, std::string, std::string) {
  std::unique_ptr<invalidation::FakeInvalidationService> invalidation_service(
      new invalidation::FakeInvalidationService);
  invalidation_service->SetInvalidatorState(
      invalidation::InvalidatorState::kDisabled);
  return invalidation_service;
}

std::unique_ptr<KeyedService> BuildProfileInvalidationProvider(
    content::BrowserContext* context) {
  return std::make_unique<invalidation::ProfileInvalidationProvider>(
      nullptr, base::BindRepeating(&CreateInvalidationServiceForSenderId));
}

void SendInvalidatorStateChangeNotification(
    invalidation::InvalidationService* service,
    invalidation::InvalidatorState state) {
  static_cast<invalidation::FCMInvalidationService*>(service)
      ->OnInvalidatorStateChange(state);
}

}  // namespace

// A simple AffiliatedInvalidationServiceProvider::Consumer that registers a
// invalidation::FakeInvalidationHandler with
// the invalidation::InvalidationService that is currently being made available.
class FakeConsumer : public AffiliatedInvalidationServiceProvider::Consumer {
 public:
  FakeConsumer(AffiliatedInvalidationServiceProviderImpl* provider,
               const std::string& invalidation_owner_name);

  FakeConsumer(const FakeConsumer&) = delete;
  FakeConsumer& operator=(const FakeConsumer&) = delete;

  ~FakeConsumer() override;

  // AffiliatedInvalidationServiceProvider::Consumer:
  void OnInvalidationServiceSet(
      invalidation::InvalidationService* invalidation_service) override;

  int GetAndClearInvalidationServiceSetCount();
  const invalidation::InvalidationService* GetInvalidationService() const;

 private:
  raw_ptr<AffiliatedInvalidationServiceProviderImpl> provider_;
  invalidation::FakeInvalidationHandler invalidation_handler_;

  int invalidation_service_set_count_ = 0;
  raw_ptr<invalidation::InvalidationService, DanglingUntriaged>
      invalidation_service_ = nullptr;
};

class AffiliatedInvalidationServiceProviderImplTest : public testing::Test {
 public:
  AffiliatedInvalidationServiceProviderImplTest();
  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // Both functions don't pass ownership of the profile. The Profile is owned
  // by the global ProfileManager.
  Profile* LogInAndReturnAffiliatedProfile(const std::string& user_id);
  Profile* LogInAndReturnNonAffiliatedProfile(const std::string& user_id);

  // Logs in as an affiliated user and indicates that the per-profile
  // invalidation service for this user connected. Verifies that this
  // invalidation service is made available to the |consumer_| and the
  // device-global invalidation service is destroyed.
  void LogInAsAffiliatedUserAndConnectInvalidationService();

  // Logs in as an unaffiliated user and indicates that the per-profile
  // invalidation service for this user connected. Verifies that this
  // invalidation service is ignored and the device-global invalidation service
  // is not destroyed.
  void LogInAsUnaffiliatedUserAndConnectInvalidationService();

  // Indicates that the device-global invalidation service connected. Verifies
  // that the |consumer_| is informed about this.
  void ConnectDeviceGlobalInvalidationService();

  // Indicates that the logged-in user's per-profile invalidation service
  // disconnected. Verifies that the |consumer_| is informed about this and a
  // device-global invalidation service is created.
  void DisconnectPerProfileInvalidationService();

  invalidation::FakeInvalidationService* GetProfileInvalidationService(
      Profile* profile,
      bool create);

 protected:
  // Ownership is not passed. The Profile is owned by the global ProfileManager.
  Profile* LogInAndReturnProfile(const std::string& user_id,
                                 bool is_affiliated);
  std::unique_ptr<AffiliatedInvalidationServiceProviderImpl> provider_;
  std::unique_ptr<FakeConsumer> consumer_;
  raw_ptr<invalidation::InvalidationService, DanglingUntriaged>
      device_invalidation_service_;
  raw_ptr<invalidation::FakeInvalidationService, DanglingUntriaged>
      profile_invalidation_service_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged> fake_user_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  TestingProfileManager profile_manager_;
  session_manager::SessionManager session_manager_;
};

FakeConsumer::FakeConsumer(AffiliatedInvalidationServiceProviderImpl* provider,
                           const std::string& invalidation_owner_name)
    : provider_(provider), invalidation_handler_(invalidation_owner_name) {
  provider_->RegisterConsumer(this);
}

FakeConsumer::~FakeConsumer() {
  if (invalidation_service_) {
    invalidation_service_->RemoveObserver(&invalidation_handler_);
  }
  provider_->UnregisterConsumer(this);

  EXPECT_EQ(0, invalidation_service_set_count_);
}

void FakeConsumer::OnInvalidationServiceSet(
    invalidation::InvalidationService* invalidation_service) {
  ++invalidation_service_set_count_;

  if (invalidation_service_) {
    invalidation_service_->RemoveObserver(&invalidation_handler_);
  }

  invalidation_service_ = invalidation_service;

  if (invalidation_service_) {
    // Regression test for http://crbug.com/455504: The |invalidation_service|
    // was sometimes destroyed without notifying consumers and giving them a
    // chance to unregister their invalidation handlers. Register an
    // invalidation handler so that |invalidation_service| CHECK()s in its
    // destructor if this regresses.
    invalidation_service_->AddObserver(&invalidation_handler_);
  }
}

int FakeConsumer::GetAndClearInvalidationServiceSetCount() {
  const int invalidation_service_set_count = invalidation_service_set_count_;
  invalidation_service_set_count_ = 0;
  return invalidation_service_set_count;
}

const invalidation::InvalidationService* FakeConsumer::GetInvalidationService()
    const {
  return invalidation_service_;
}

AffiliatedInvalidationServiceProviderImplTest::
    AffiliatedInvalidationServiceProviderImplTest()
    : device_invalidation_service_(nullptr),
      profile_invalidation_service_(nullptr),
      fake_user_manager_(new ash::FakeChromeUserManager),
      user_manager_enabler_(base::WrapUnique(fake_user_manager_.get())),
      profile_manager_(TestingBrowserProcess::GetGlobal()) {
  cros_settings_test_helper_.InstallAttributes()->SetCloudManaged("example.com",
                                                                  "device_id");
}

void AffiliatedInvalidationServiceProviderImplTest::SetUp() {
  ash::CryptohomeMiscClient::InitializeFake();
  ash::SystemSaltGetter::Initialize();
  ASSERT_TRUE(profile_manager_.SetUp());

  DeviceOAuth2TokenServiceFactory::Initialize(
      test_url_loader_factory_.GetSafeWeakWrapper(),
      TestingBrowserProcess::GetGlobal()->local_state());

  invalidation::ProfileInvalidationProviderFactory::GetInstance()
      ->RegisterTestingFactory(
          base::BindRepeating(&BuildProfileInvalidationProvider));

  provider_ = std::make_unique<AffiliatedInvalidationServiceProviderImpl>();
}

void AffiliatedInvalidationServiceProviderImplTest::TearDown() {
  consumer_.reset();
  provider_->Shutdown();
  provider_.reset();

  invalidation::ProfileInvalidationProviderFactory::GetInstance()
      ->RegisterTestingFactory(
          invalidation::ProfileInvalidationProviderFactory::
              GlobalTestingFactory());
  DeviceOAuth2TokenServiceFactory::Shutdown();
  ash::SystemSaltGetter::Shutdown();
  ash::CryptohomeMiscClient::Shutdown();
}

Profile*
AffiliatedInvalidationServiceProviderImplTest::LogInAndReturnAffiliatedProfile(
    const std::string& user_id) {
  return LogInAndReturnProfile(user_id, true);
}

Profile* AffiliatedInvalidationServiceProviderImplTest::
    LogInAndReturnNonAffiliatedProfile(const std::string& user_id) {
  return LogInAndReturnProfile(user_id, false);
}

Profile* AffiliatedInvalidationServiceProviderImplTest::LogInAndReturnProfile(
    const std::string& user_id,
    bool is_affiliated) {
  TestingProfile* profile = profile_manager_.CreateTestingProfile(user_id);
  AccountId account_id = AccountId::FromUserEmail(user_id);
  fake_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
      account_id, is_affiliated, user_manager::UserType::kRegular, profile);
  session_manager_.NotifyUserProfileLoaded(account_id);
  return profile;
}

void AffiliatedInvalidationServiceProviderImplTest::
    LogInAsAffiliatedUserAndConnectInvalidationService() {
  // Log in as an affiliated user.
  Profile* profile = LogInAndReturnAffiliatedProfile(kAffiliatedUserID1);
  EXPECT_TRUE(profile);

  // Verify that a per-profile invalidation service has been created.
  profile_invalidation_service_ =
      GetProfileInvalidationService(profile, false /* create */);
  ASSERT_TRUE(profile_invalidation_service_);

  // Verify that the device-global invalidation service still exists.
  EXPECT_TRUE(provider_->GetDeviceInvalidationServiceForTest());

  // Indicate that the per-profile invalidation service has connected. Verify
  // that the consumer is informed about this.
  EXPECT_EQ(0, consumer_->GetAndClearInvalidationServiceSetCount());
  profile_invalidation_service_->SetInvalidatorState(
      invalidation::InvalidatorState::kEnabled);
  EXPECT_EQ(1, consumer_->GetAndClearInvalidationServiceSetCount());
  EXPECT_EQ(profile_invalidation_service_, consumer_->GetInvalidationService());

  // Verify that the device-global invalidation service has been destroyed.
  EXPECT_FALSE(provider_->GetDeviceInvalidationServiceForTest());
}

void AffiliatedInvalidationServiceProviderImplTest::
    LogInAsUnaffiliatedUserAndConnectInvalidationService() {
  // Log in as an unaffiliated user.
  Profile* profile = LogInAndReturnNonAffiliatedProfile(kUnaffiliatedUserID);
  EXPECT_TRUE(profile);

  // Verify that a per-profile invalidation service has been created.
  profile_invalidation_service_ =
      GetProfileInvalidationService(profile, false /* create */);
  ASSERT_TRUE(profile_invalidation_service_);

  // Verify that the device-global invalidation service still exists.
  EXPECT_TRUE(provider_->GetDeviceInvalidationServiceForTest());

  // Indicate that the per-profile invalidation service has connected. Verify
  // that the consumer is not called back.
  profile_invalidation_service_->SetInvalidatorState(
      invalidation::InvalidatorState::kEnabled);
  EXPECT_EQ(0, consumer_->GetAndClearInvalidationServiceSetCount());

  // Verify that the device-global invalidation service still exists.
  EXPECT_TRUE(provider_->GetDeviceInvalidationServiceForTest());
}

void AffiliatedInvalidationServiceProviderImplTest::
    ConnectDeviceGlobalInvalidationService() {
  // Verify that a device-global invalidation service has been created.
  device_invalidation_service_ =
      provider_->GetDeviceInvalidationServiceForTest();
  ASSERT_TRUE(device_invalidation_service_);

  // Indicate that the device-global invalidation service has connected. Verify
  // that the consumer is informed about this.
  EXPECT_EQ(0, consumer_->GetAndClearInvalidationServiceSetCount());
  SendInvalidatorStateChangeNotification(
      device_invalidation_service_, invalidation::InvalidatorState::kEnabled);
  EXPECT_EQ(1, consumer_->GetAndClearInvalidationServiceSetCount());
  EXPECT_EQ(device_invalidation_service_, consumer_->GetInvalidationService());
}

void AffiliatedInvalidationServiceProviderImplTest::
    DisconnectPerProfileInvalidationService() {
  ASSERT_TRUE(profile_invalidation_service_);

  // Indicate that the per-profile invalidation service has disconnected. Verify
  // that the consumer is informed about this.
  EXPECT_EQ(0, consumer_->GetAndClearInvalidationServiceSetCount());
  profile_invalidation_service_->SetInvalidatorState(
      invalidation::InvalidatorState::kDisabled);
  EXPECT_EQ(1, consumer_->GetAndClearInvalidationServiceSetCount());
  EXPECT_EQ(nullptr, consumer_->GetInvalidationService());

  // Verify that a device-global invalidation service has been created.
  EXPECT_TRUE(provider_->GetDeviceInvalidationServiceForTest());
}

invalidation::FakeInvalidationService*
AffiliatedInvalidationServiceProviderImplTest::GetProfileInvalidationService(
    Profile* profile,
    bool create) {
  invalidation::ProfileInvalidationProvider* invalidation_provider;
  invalidation_provider =
      static_cast<invalidation::ProfileInvalidationProvider*>(
          invalidation::ProfileInvalidationProviderFactory::GetInstance()
              ->GetServiceForBrowserContext(profile, create));
  if (!invalidation_provider)
    return nullptr;
  auto invalidation_service =
      invalidation_provider->GetInvalidationServiceOrListener(
          kPolicyFCMInvalidationSenderID,
          invalidation::InvalidationListener::kProjectNumberEnterprise);
  CHECK(std::holds_alternative<invalidation::InvalidationService*>(
      invalidation_service));
  return static_cast<invalidation::FakeInvalidationService*>(
      std::get<invalidation::InvalidationService*>(invalidation_service));
}

// No consumers are registered with the
// AffiliatedInvalidationServiceProviderImpl. Verifies that no device-global
// invalidation service is created, whether an affiliated user is logged in or
// not.
TEST_F(AffiliatedInvalidationServiceProviderImplTest, NoConsumers) {
  // Verify that no device-global invalidation service has been created.
  EXPECT_FALSE(provider_->GetDeviceInvalidationServiceForTest());

  // Log in as an affiliated user.
  EXPECT_TRUE(LogInAndReturnAffiliatedProfile(kAffiliatedUserID1));

  // Verify that no device-global invalidation service has been created.
  EXPECT_FALSE(provider_->GetDeviceInvalidationServiceForTest());
}

// Verifies that when no connected invalidation service is available for use,
// none is made available to consumers.
TEST_F(AffiliatedInvalidationServiceProviderImplTest,
       NoInvalidationServiceAvailable) {
  // Register a consumer. Verify that the consumer is not called back
  // immediately as no connected invalidation service exists yet.
  consumer_ = std::make_unique<FakeConsumer>(provider_.get(), "consumer");
  EXPECT_EQ(0, consumer_->GetAndClearInvalidationServiceSetCount());
}

// A consumer is registered with the AffiliatedInvalidationServiceProviderImpl.
// Verifies that when no per-profile invalidation service belonging to an
// affiliated user is available, a device-global invalidation service is
// created. Further verifies that when the device-global invalidation service
// connects, it is made available to the consumer.
TEST_F(AffiliatedInvalidationServiceProviderImplTest,
       UseDeviceInvalidationService) {
  consumer_ = std::make_unique<FakeConsumer>(provider_.get(), "consumer");

  // Indicate that the device-global invalidation service connected. Verify that
  // that the consumer is informed about this.
  ConnectDeviceGlobalInvalidationService();

  // Indicate that the device-global invalidation service has disconnected.
  // Verify that the consumer is informed about this.
  EXPECT_EQ(0, consumer_->GetAndClearInvalidationServiceSetCount());
  SendInvalidatorStateChangeNotification(
      device_invalidation_service_, invalidation::InvalidatorState::kDisabled);
  EXPECT_EQ(1, consumer_->GetAndClearInvalidationServiceSetCount());
  EXPECT_EQ(nullptr, consumer_->GetInvalidationService());

  // Verify that the device-global invalidation service still exists.
  EXPECT_TRUE(provider_->GetDeviceInvalidationServiceForTest());
}

// A consumer is registered with the AffiliatedInvalidationServiceProviderImpl.
// Verifies that when a per-profile invalidation service belonging to an
// affiliated user connects, it is made available to the consumer.
TEST_F(AffiliatedInvalidationServiceProviderImplTest,
       UseAffiliatedProfileInvalidationService) {
  consumer_ = std::make_unique<FakeConsumer>(provider_.get(), "consumer");

  // Verify that a device-global invalidation service has been created.
  EXPECT_TRUE(provider_->GetDeviceInvalidationServiceForTest());

  // Log in as an affiliated user and indicate that the per-profile invalidation
  // service for this user connected. Verify that this invalidation service is
  // made available to the |consumer_| and the device-global invalidation
  // service is destroyed.
  LogInAsAffiliatedUserAndConnectInvalidationService();

  // Indicate that the logged-in user's per-profile invalidation service
  // disconnected. Verify that the consumer is informed about this and a
  // device-global invalidation service is created.
  DisconnectPerProfileInvalidationService();
}

// A consumer is registered with the AffiliatedInvalidationServiceProviderImpl.
// Verifies that when a per-profile invalidation service belonging to an
// unaffiliated user connects, it is ignored.
TEST_F(AffiliatedInvalidationServiceProviderImplTest,
       DoNotUseUnaffiliatedProfileInvalidationService) {
  consumer_ = std::make_unique<FakeConsumer>(provider_.get(), "consumer");

  // Verify that a device-global invalidation service has been created.
  EXPECT_TRUE(provider_->GetDeviceInvalidationServiceForTest());

  // Log in as an unaffiliated user and indicate that the per-profile
  // invalidation service for this user connected. Verify that this invalidation
  // service is ignored and the device-global invalidation service is not
  // destroyed.
  LogInAsUnaffiliatedUserAndConnectInvalidationService();
}

// A consumer is registered with the AffiliatedInvalidationServiceProviderImpl.
// A device-global invalidation service exists, is connected and is made
// available to the consumer. Verifies that when a per-profile invalidation
// service belonging to an affiliated user connects, it is made available to the
// consumer instead and the device-global invalidation service is destroyed.
TEST_F(AffiliatedInvalidationServiceProviderImplTest,
       SwitchToAffiliatedProfileInvalidationService) {
  consumer_ = std::make_unique<FakeConsumer>(provider_.get(), "consumer");

  // Indicate that the device-global invalidation service connected. Verify that
  // that the consumer is informed about this.
  ConnectDeviceGlobalInvalidationService();

  // Log in as an affiliated user and indicate that the per-profile invalidation
  // service for this user connected. Verify that this invalidation service is
  // made available to the |consumer_| and the device-global invalidation
  // service is destroyed.
  LogInAsAffiliatedUserAndConnectInvalidationService();
}

// Verifies that every InvalidationService state except
// |invalidation::InvalidatorState::kEnabled| are treated as disconnected.
TEST_F(AffiliatedInvalidationServiceProviderImplTest,
       FlipInvalidationServiceState) {
  consumer_ = std::make_unique<FakeConsumer>(provider_.get(), "consumer");

  // Create and make |profile_invalidation_service_| enabled.
  LogInAsAffiliatedUserAndConnectInvalidationService();

  profile_invalidation_service_->SetInvalidatorState(
      invalidation::InvalidatorState::kDisabled);
  EXPECT_EQ(1, consumer_->GetAndClearInvalidationServiceSetCount());
  EXPECT_FALSE(consumer_->GetInvalidationService());

  profile_invalidation_service_->SetInvalidatorState(
      invalidation::InvalidatorState::kEnabled);
  EXPECT_EQ(1, consumer_->GetAndClearInvalidationServiceSetCount());
  EXPECT_EQ(profile_invalidation_service_, consumer_->GetInvalidationService());
}

// A consumer is registered with the AffiliatedInvalidationServiceProviderImpl.
// A device-global invalidation service exists, is connected and is made
// available to the consumer. Verifies that when a per-profile invalidation
// service belonging to an unaffiliated user connects, it is ignored and the
// device-global invalidation service continues to be made available to the
// consumer.
TEST_F(AffiliatedInvalidationServiceProviderImplTest,
       DoNotSwitchToUnaffiliatedProfileInvalidationService) {
  consumer_ = std::make_unique<FakeConsumer>(provider_.get(), "consumer");

  // Indicate that the device-global invalidation service connected. Verify that
  // that the consumer is informed about this.
  ConnectDeviceGlobalInvalidationService();

  // Log in as an unaffiliated user and indicate that the per-profile
  // invalidation service for this user connected. Verify that this invalidation
  // service is ignored and the device-global invalidation service is not
  // destroyed.
  LogInAsUnaffiliatedUserAndConnectInvalidationService();
}

// A consumer is registered with the AffiliatedInvalidationServiceProviderImpl.
// A per-profile invalidation service belonging to an affiliated user exists, is
// connected and is made available to the consumer. Verifies that when the
// per-profile invalidation service disconnects, a device-global invalidation
// service is created. Further verifies that when the device-global invalidation
// service connects, it is made available to the consumer.
TEST_F(AffiliatedInvalidationServiceProviderImplTest,
       SwitchToDeviceInvalidationService) {
  consumer_ = std::make_unique<FakeConsumer>(provider_.get(), "consumer");

  // Verify that a device-global invalidation service has been created.
  EXPECT_TRUE(provider_->GetDeviceInvalidationServiceForTest());

  // Log in as an affiliated user and indicate that the per-profile invalidation
  // service for this user connected. Verify that this invalidation service is
  // made available to the |consumer_| and the device-global invalidation
  // service is destroyed.
  LogInAsAffiliatedUserAndConnectInvalidationService();

  // Indicate that the logged-in user's per-profile invalidation service
  // disconnected. Verify that the consumer is informed about this and a
  // device-global invalidation service is created.
  DisconnectPerProfileInvalidationService();

  // Indicate that the device-global invalidation service connected. Verify that
  // that the consumer is informed about this.
  ConnectDeviceGlobalInvalidationService();
}

// A consumer is registered with the AffiliatedInvalidationServiceProviderImpl.
// A per-profile invalidation service belonging to a first affiliated user
// exists, is connected and is made available to the consumer. A per-profile
// invalidation service belonging to a second affiliated user also exists and is
// connected. Verifies that when the per-profile invalidation service belonging
// to the first user disconnects, the per-profile invalidation service belonging
// to the second user is made available to the consumer instead.
TEST_F(AffiliatedInvalidationServiceProviderImplTest,
       SwitchBetweenAffiliatedProfileInvalidationServices) {
  consumer_ = std::make_unique<FakeConsumer>(provider_.get(), "consumer");

  // Verify that a device-global invalidation service has been created.
  EXPECT_TRUE(provider_->GetDeviceInvalidationServiceForTest());

  // Log in as a first affiliated user and indicate that the per-profile
  // invalidation service for this user connected. Verify that this invalidation
  // service is made available to the |consumer_| and the device-global
  // invalidation service is destroyed.
  LogInAsAffiliatedUserAndConnectInvalidationService();

  // Log in as a second affiliated user.
  Profile* second_profile = LogInAndReturnAffiliatedProfile(kAffiliatedUserID2);
  EXPECT_TRUE(second_profile);

  // Verify that the device-global invalidation service still does not exist.
  EXPECT_FALSE(provider_->GetDeviceInvalidationServiceForTest());

  // Verify that a per-profile invalidation service for the second user has been
  // created.
  invalidation::FakeInvalidationService* second_profile_invalidation_service =
      GetProfileInvalidationService(second_profile, false /* create */);
  ASSERT_TRUE(second_profile_invalidation_service);

  // Indicate that the second user's per-profile invalidation service has
  // connected. Verify that the consumer is not called back.
  second_profile_invalidation_service->SetInvalidatorState(
      invalidation::InvalidatorState::kEnabled);
  EXPECT_EQ(0, consumer_->GetAndClearInvalidationServiceSetCount());

  // Indicate that the first user's per-profile invalidation service has
  // disconnected. Verify that the consumer is informed that the second user's
  // per-profile invalidation service should be used instead of the first
  // user's.
  EXPECT_EQ(0, consumer_->GetAndClearInvalidationServiceSetCount());
  profile_invalidation_service_->SetInvalidatorState(
      invalidation::InvalidatorState::kDisabled);
  EXPECT_EQ(1, consumer_->GetAndClearInvalidationServiceSetCount());
  EXPECT_EQ(second_profile_invalidation_service,
            consumer_->GetInvalidationService());

  // Verify that the device-global invalidation service still does not exist.
  EXPECT_FALSE(provider_->GetDeviceInvalidationServiceForTest());
}

// A consumer is registered with the AffiliatedInvalidationServiceProviderImpl.
// A device-global invalidation service exists, is connected and is made
// available to the consumer. Verifies that when a second consumer registers,
// the device-global invalidation service is made available to it as well.
// Further verifies that when the first consumer unregisters, the device-global
// invalidation service is not destroyed and remains available to the second
// consumer. Further verifies that when the second consumer also unregisters,
// the device-global invalidation service is destroyed.
TEST_F(AffiliatedInvalidationServiceProviderImplTest, MultipleConsumers) {
  consumer_ = std::make_unique<FakeConsumer>(provider_.get(), "consumer");

  // Indicate that the device-global invalidation service connected. Verify that
  // that the consumer is informed about this.
  ConnectDeviceGlobalInvalidationService();

  // Register a second consumer. Verify that the consumer is called back
  // immediately as a connected invalidation service is available.
  std::unique_ptr<FakeConsumer> second_consumer(
      new FakeConsumer(provider_.get(), "second_consumer"));
  EXPECT_EQ(1, second_consumer->GetAndClearInvalidationServiceSetCount());
  EXPECT_EQ(device_invalidation_service_,
            second_consumer->GetInvalidationService());

  // Unregister the first consumer.
  consumer_.reset();

  // Verify that the device-global invalidation service still exists.
  EXPECT_TRUE(provider_->GetDeviceInvalidationServiceForTest());

  // Unregister the second consumer.
  second_consumer.reset();

  // Verify that the device-global invalidation service has been destroyed.
  EXPECT_FALSE(provider_->GetDeviceInvalidationServiceForTest());
}

// A consumer is registered with the AffiliatedInvalidationServiceProviderImpl.
// A per-profile invalidation service belonging to a first affiliated user
// exists, is connected and is made available to the consumer. Verifies that
// when the provider is shut down, the consumer is informed that no
// invalidation service is available for use anymore. Also verifies that no
// device-global invalidation service is created and a per-profile invalidation
// service belonging to a second affiliated user that subsequently connects is
// ignored.
TEST_F(AffiliatedInvalidationServiceProviderImplTest, NoServiceAfterShutdown) {
  consumer_ = std::make_unique<FakeConsumer>(provider_.get(), "consumer");

  // Verify that a device-global invalidation service has been created.
  EXPECT_TRUE(provider_->GetDeviceInvalidationServiceForTest());

  // Log in as a first affiliated user and indicate that the per-profile
  // invalidation service for this user connected. Verify that this invalidation
  // service is made available to the |consumer_| and the device-global
  // invalidation service is destroyed.
  LogInAsAffiliatedUserAndConnectInvalidationService();

  // Shut down the |provider_|. Verify that the |consumer_| is informed that no
  // invalidation service is available for use anymore.
  EXPECT_EQ(0, consumer_->GetAndClearInvalidationServiceSetCount());
  provider_->Shutdown();
  EXPECT_EQ(1, consumer_->GetAndClearInvalidationServiceSetCount());
  EXPECT_EQ(nullptr, consumer_->GetInvalidationService());

  // Verify that the device-global invalidation service still does not exist.
  EXPECT_FALSE(provider_->GetDeviceInvalidationServiceForTest());

  // Log in as a second affiliated user.
  Profile* second_profile = LogInAndReturnAffiliatedProfile(kAffiliatedUserID2);
  EXPECT_TRUE(second_profile);

  // Verify that the device-global invalidation service still does not exist.
  EXPECT_FALSE(provider_->GetDeviceInvalidationServiceForTest());

  // Create a per-profile invalidation service for the second user.
  invalidation::FakeInvalidationService* second_profile_invalidation_service =
      GetProfileInvalidationService(second_profile, true /* create */);
  ASSERT_TRUE(second_profile_invalidation_service);

  // Indicate that the second user's per-profile invalidation service has
  // connected. Verify that the consumer is not called back.
  second_profile_invalidation_service->SetInvalidatorState(
      invalidation::InvalidatorState::kEnabled);
  EXPECT_EQ(0, consumer_->GetAndClearInvalidationServiceSetCount());

  // Verify that the device-global invalidation service still does not exist.
  EXPECT_FALSE(provider_->GetDeviceInvalidationServiceForTest());
}

// A consumer is registered with the AffiliatedInvalidationServiceProviderImpl.
// A device-global invalidation service exists, is connected and is made
// available to the consumer. Verifies that when the provider is shut down, the
// consumer is informed that no invalidation service is available for use
// anymore before the device-global invalidation service is destroyed.
// This is a regression test for http://crbug.com/455504.
TEST_F(AffiliatedInvalidationServiceProviderImplTest,
       ConnectedDeviceGlobalInvalidationServiceOnShutdown) {
  consumer_ = std::make_unique<FakeConsumer>(provider_.get(), "consumer");

  // Verify that a device-global invalidation service has been created.
  EXPECT_TRUE(provider_->GetDeviceInvalidationServiceForTest());

  // Indicate that the device-global invalidation service connected. Verify that
  // that the consumer is informed about this.
  ConnectDeviceGlobalInvalidationService();

  // Shut down the |provider_|. Verify that the |consumer_| is informed that no
  // invalidation service is available for use anymore. This also serves as a
  // regression test which verifies that the invalidation service is not
  // destroyed until the |consumer_| has been informed: If the invalidation
  // service was destroyed too early, the |consumer_| would still be registered
  // as an observer and the invalidation service's destructor would DCHECK().
  EXPECT_EQ(0, consumer_->GetAndClearInvalidationServiceSetCount());
  provider_->Shutdown();
  EXPECT_EQ(1, consumer_->GetAndClearInvalidationServiceSetCount());
  EXPECT_EQ(nullptr, consumer_->GetInvalidationService());

  // Verify that the device-global invalidation service has been destroyed.
  EXPECT_FALSE(provider_->GetDeviceInvalidationServiceForTest());
}

}  // namespace policy
