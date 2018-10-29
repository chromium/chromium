// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_app_manager.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_notification_controller.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service_factory.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service_regular.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "components/account_id/account_id.h"
#include "components/signin/core/browser/account_info.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"

using device::MockBluetoothAdapter;
using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace chromeos {

namespace {

// Emails for fake users used in tests.
const char kTestUserPrimary[] = "primary_user@nowhere.com";
const char kTestUserSecondary[] = "secondary_user@nowhere.com";

class MockEasyUnlockNotificationController
    : public EasyUnlockNotificationController {
 public:
  MockEasyUnlockNotificationController()
      : EasyUnlockNotificationController(nullptr) {}
  ~MockEasyUnlockNotificationController() override {}

  // EasyUnlockNotificationController:
  MOCK_METHOD0(ShowChromebookAddedNotification, void());
  MOCK_METHOD0(ShowPairingChangeNotification, void());
  MOCK_METHOD1(ShowPairingChangeAppliedNotification, void(const std::string&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEasyUnlockNotificationController);
};

// App manager to be used in EasyUnlockService tests.
// This effectivelly abstracts the extension system from the tests.
class TestAppManager : public EasyUnlockAppManager {
 public:
  TestAppManager()
      : state_(STATE_NOT_LOADED),
        app_launch_count_(0u),
        reload_count_(0u),
        ready_(false) {}
  ~TestAppManager() override {}

  // The easy unlock app state.
  enum State { STATE_NOT_LOADED, STATE_LOADED, STATE_DISABLED };

  State state() const { return state_; }
  size_t app_launch_count() const { return app_launch_count_; }
  size_t reload_count() const { return reload_count_; }

  // Marks the manager as ready and runs |ready_callback_| if there is one set.
  void SetReady() {
    ready_ = true;
    if (!ready_callback_.is_null()) {
      ready_callback_.Run();
      ready_callback_ = base::Closure();
    }
  }

  void EnsureReady(const base::Closure& ready_callback) override {
    ASSERT_TRUE(ready_callback_.is_null());
    if (ready_) {
      ready_callback.Run();
      return;
    }
    ready_callback_ = ready_callback;
  }

  void LaunchSetup() override {
    ASSERT_EQ(STATE_LOADED, state_);
    ++app_launch_count_;
  }

  void LoadApp() override { state_ = STATE_LOADED; }

  void DisableAppIfLoaded() override {
    if (state_ == STATE_LOADED)
      state_ = STATE_DISABLED;
  }

  void ReloadApp() override {
    if (state_ == STATE_LOADED)
      ++reload_count_;
  }

  bool SendAuthAttemptEvent() override {
    ADD_FAILURE() << "Not reached.";
    return false;
  }

 private:
  // The current app state.
  State state_;

  // Number of times LaunchSetup was called.
  size_t app_launch_count_;

  // Number of times ReloadApp was called.
  size_t reload_count_;

  // Whether the manager is ready. Set using |SetReady|.
  bool ready_;
  // If |EnsureReady| was called before |SetReady|, cached callback that will be
  // called when manager becomes ready.
  base::Closure ready_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestAppManager);
};

// Helper factory that tracks AppManagers passed to EasyUnlockServices per
// browser context owning a EasyUnlockService. Used to allow tests access to the
// TestAppManagers passed to the created services.
class TestAppManagerFactory {
 public:
  TestAppManagerFactory() {}
  ~TestAppManagerFactory() {}

  // Creates a TestAppManager for the provided browser context. If a
  // TestAppManager was already created for the context, returns NULL.
  std::unique_ptr<TestAppManager> Create(content::BrowserContext* context) {
    if (Find(context))
      return std::unique_ptr<TestAppManager>();
    std::unique_ptr<TestAppManager> app_manager(new TestAppManager());
    mapping_[context] = app_manager.get();
    return app_manager;
  }

  // Finds a TestAppManager created for |context|. Returns NULL if no
  // TestAppManagers have been created for the context.
  TestAppManager* Find(content::BrowserContext* context) {
    std::map<content::BrowserContext*, TestAppManager*>::iterator it =
        mapping_.find(context);
    if (it == mapping_.end())
      return NULL;
    return it->second;
  }

 private:
  // Mapping from browser contexts to test AppManagers. The AppManagers are not
  // owned by this.
  std::map<content::BrowserContext*, TestAppManager*> mapping_;

  DISALLOW_COPY_AND_ASSIGN(TestAppManagerFactory);
};

// Global TestAppManager factory. It should be created and destroyed in
// EasyUnlockServiceTest::SetUp and EasyUnlockServiceTest::TearDown,
// respectively.
TestAppManagerFactory* app_manager_factory = nullptr;

device_sync::FakeDeviceSyncClient* GetDefaultDeviceSyncClient() {
  static base::NoDestructor<device_sync::FakeDeviceSyncClient> fake_client;
  return fake_client.get();
}

multidevice_setup::FakeMultiDeviceSetupClient*
GetDefaultMultiDeviceSetupClient() {
  static base::NoDestructor<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_client;
  return fake_client.get();
}

// EasyUnlockService factory function injected into testing profiles.
// It creates an EasyUnlockService with test AppManager.
std::unique_ptr<KeyedService> CreateEasyUnlockServiceForTest(
    content::BrowserContext* context) {
  EXPECT_TRUE(app_manager_factory);
  if (!app_manager_factory)
    return nullptr;

  std::unique_ptr<EasyUnlockAppManager> app_manager =
      app_manager_factory->Create(context);
  EXPECT_TRUE(app_manager.get());
  if (!app_manager.get())
    return nullptr;

  std::unique_ptr<EasyUnlockServiceRegular> service(
      new EasyUnlockServiceRegular(
          Profile::FromBrowserContext(context),
          nullptr /* secure_channel_client */,
          std::make_unique<MockEasyUnlockNotificationController>(),
          GetDefaultDeviceSyncClient(), GetDefaultMultiDeviceSetupClient()));
  service->Initialize(std::move(app_manager));
  return std::move(service);
}

}  // namespace

class EasyUnlockServiceTest : public testing::Test {
 protected:
  EasyUnlockServiceTest()
      : mock_user_manager_(new testing::NiceMock<MockUserManager>()),
        scoped_user_manager_(base::WrapUnique(mock_user_manager_)),
        is_bluetooth_adapter_present_(true) {}

  ~EasyUnlockServiceTest() override {}

  void SetUp() override {
    app_manager_factory = new TestAppManagerFactory();

    mock_adapter_ = new testing::NiceMock<MockBluetoothAdapter>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);
    EXPECT_CALL(*mock_adapter_, IsPresent())
        .WillRepeatedly(testing::Invoke(
            this, &EasyUnlockServiceTest::is_bluetooth_adapter_present));

    std::unique_ptr<DBusThreadManagerSetter> dbus_setter =
        DBusThreadManager::GetSetterForTesting();
    power_manager_client_ = new FakePowerManagerClient;
    dbus_setter->SetPowerManagerClient(
        std::unique_ptr<PowerManagerClient>(power_manager_client_));

    ON_CALL(*mock_user_manager_, Shutdown()).WillByDefault(Return());
    ON_CALL(*mock_user_manager_, IsLoggedInAsUserWithGaiaAccount())
        .WillByDefault(Return(true));
    ON_CALL(*mock_user_manager_, IsCurrentUserNonCryptohomeDataEphemeral())
        .WillByDefault(Return(false));

    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_pref_service_);
    RegisterLocalState(local_pref_service_.registry());

    profile_ = SetUpProfile(kTestUserPrimary, &profile_gaia_id_);
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);

    delete app_manager_factory;
    app_manager_factory = nullptr;
  }

  void SetEasyUnlockAllowedPolicy(bool allowed) {
    profile_->GetTestingPrefService()->SetManagedPref(
        prefs::kEasyUnlockAllowed, std::make_unique<base::Value>(allowed));
  }

  void set_is_bluetooth_adapter_present(bool is_present) {
    is_bluetooth_adapter_present_ = is_present;
  }

  bool is_bluetooth_adapter_present() const {
    return is_bluetooth_adapter_present_;
  }

  FakePowerManagerClient* power_manager_client() {
    return power_manager_client_;
  }

  // Checks whether AppManager passed to EasyUnlockservice for |profile| has
  // Easy Unlock app loaded.
  bool EasyUnlockAppInState(Profile* profile, TestAppManager::State state) {
    EXPECT_TRUE(app_manager_factory);
    if (!app_manager_factory)
      return false;
    TestAppManager* app_manager = app_manager_factory->Find(profile);
    EXPECT_TRUE(app_manager);
    return app_manager && app_manager->state() == state;
  }

  void SetAppManagerReady(content::BrowserContext* context) {
    ASSERT_TRUE(app_manager_factory);
    TestAppManager* app_manager = app_manager_factory->Find(context);
    ASSERT_TRUE(app_manager);
    app_manager->SetReady();
  }

  // Sets up a test profile using the provided |email|. Will generate a unique
  // gaia id and output to |gaia_id|. Returns the created TestingProfile.
  std::unique_ptr<TestingProfile> SetUpProfile(const std::string& email,
                                               std::string* gaia_id) {
    std::unique_ptr<TestingProfile> profile =
        IdentityTestEnvironmentProfileAdaptor::
            CreateProfileForIdentityTestEnvironment(
                {{EasyUnlockServiceFactory::GetInstance(),
                  base::BindRepeating(&CreateEasyUnlockServiceForTest)}});

    // Note: This can simply be a local variable as the test does not need to
    // interact with IdentityTestEnvironment outside of this method. If that
    // ever changes, there will need to be distinct instance variables for the
    // environments associated with |profile_| and |secondary_profile_|.
    IdentityTestEnvironmentProfileAdaptor identity_test_env_adaptor(
        profile.get());
    AccountInfo account_info =
        identity_test_env_adaptor.identity_test_env()->SetPrimaryAccount(email);

    *gaia_id = account_info.gaia;

    mock_user_manager_->AddUser(
        AccountId::FromUserEmailGaiaId(email, *gaia_id));
    profile.get()->set_profile_name(email);

    return profile;
  }

  void SetUpSecondaryProfile() {
    secondary_profile_ =
        SetUpProfile(kTestUserSecondary, &secondary_profile_gaia_id_);
  }

  // Must outlive TestingProfiles.
  content::TestBrowserThreadBundle thread_bundle_;

  std::unique_ptr<TestingProfile> profile_;
  std::string profile_gaia_id_;
  std::unique_ptr<TestingProfile> secondary_profile_;
  std::string secondary_profile_gaia_id_;
  MockUserManager* mock_user_manager_;

 private:
  user_manager::ScopedUserManager scoped_user_manager_;

  FakePowerManagerClient* power_manager_client_;

  bool is_bluetooth_adapter_present_;
  scoped_refptr<testing::NiceMock<MockBluetoothAdapter>> mock_adapter_;

  // PrefService which contains the browser process' local storage.
  TestingPrefServiceSimple local_pref_service_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockServiceTest);
};

TEST_F(EasyUnlockServiceTest, NoBluetoothNoService) {
  set_is_bluetooth_adapter_present(false);

  // This should start easy unlock service initialization.
  SetAppManagerReady(profile_.get());

  EasyUnlockService* service = EasyUnlockService::Get(profile_.get());
  ASSERT_TRUE(service);

  EXPECT_FALSE(service->IsAllowed());
  EXPECT_TRUE(
      EasyUnlockAppInState(profile_.get(), TestAppManager::STATE_NOT_LOADED));
}

// TODO(https://crbug.com/893878): Fix disabled test.
TEST_F(EasyUnlockServiceTest, DISABLED_DisabledOnSuspend) {
  // This should start easy unlock service initialization.
  SetAppManagerReady(profile_.get());

  EasyUnlockService* service = EasyUnlockService::Get(profile_.get());
  ASSERT_TRUE(service);

  EXPECT_TRUE(service->IsAllowed());
  EXPECT_TRUE(
      EasyUnlockAppInState(profile_.get(), TestAppManager::STATE_LOADED));

  power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_TRUE(
      EasyUnlockAppInState(profile_.get(), TestAppManager::STATE_DISABLED));

  power_manager_client()->SendSuspendDone();
  EXPECT_TRUE(
      EasyUnlockAppInState(profile_.get(), TestAppManager::STATE_LOADED));
}

// TODO(https://crbug.com/893878): Fix disabled test.
TEST_F(EasyUnlockServiceTest, DISABLED_NotAllowedForSecondaryProfile) {
  SetAppManagerReady(profile_.get());

  EasyUnlockService* primary_service = EasyUnlockService::Get(profile_.get());
  ASSERT_TRUE(primary_service);

  // A sanity check for the test to confirm that the primary profile service
  // is allowed under these conditions..
  ASSERT_TRUE(primary_service->IsAllowed());

  SetUpSecondaryProfile();
  SetAppManagerReady(secondary_profile_.get());

  EasyUnlockService* secondary_service =
      EasyUnlockService::Get(secondary_profile_.get());
  ASSERT_TRUE(secondary_service);

  EXPECT_FALSE(secondary_service->IsAllowed());
  EXPECT_TRUE(EasyUnlockAppInState(secondary_profile_.get(),
                                   TestAppManager::STATE_NOT_LOADED));
}

TEST_F(EasyUnlockServiceTest, NotAllowedForEphemeralAccounts) {
  ON_CALL(*mock_user_manager_, IsCurrentUserNonCryptohomeDataEphemeral())
      .WillByDefault(Return(true));

  SetAppManagerReady(profile_.get());
  EXPECT_FALSE(EasyUnlockService::Get(profile_.get())->IsAllowed());
  EXPECT_TRUE(
      EasyUnlockAppInState(profile_.get(), TestAppManager::STATE_NOT_LOADED));
}

TEST_F(EasyUnlockServiceTest, GetAccountId) {
  EXPECT_EQ(AccountId::FromUserEmailGaiaId(kTestUserPrimary, profile_gaia_id_),
            EasyUnlockService::Get(profile_.get())->GetAccountId());

  SetUpSecondaryProfile();
  EXPECT_EQ(AccountId::FromUserEmailGaiaId(kTestUserSecondary,
                                           secondary_profile_gaia_id_),
            EasyUnlockService::Get(secondary_profile_.get())->GetAccountId());
}

}  // namespace chromeos
