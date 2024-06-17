// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/serial_chooser_context.h"

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/serial/serial_blocklist.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/serial/serial_chooser_histograms.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/test/object_permission_context_base_mock_permission_observer.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "services/device/public/cpp/test/fake_serial_port_manager.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

namespace {

using ::base::test::InvokeFuture;
using ::base::test::ParseJson;
using ::base::test::TestFuture;
using ::content_settings::SettingSource;
using ::testing::_;
using ::testing::NiceMock;

constexpr char kTestUserEmail[] = "user@example.com";

class MockPortObserver : public SerialChooserContext::PortObserver {
 public:
  MockPortObserver() = default;
  MockPortObserver(MockPortObserver&) = delete;
  MockPortObserver& operator=(MockPortObserver&) = delete;
  ~MockPortObserver() override = default;

  MOCK_METHOD1(OnPortAdded, void(const device::mojom::SerialPortInfo&));
  MOCK_METHOD1(OnPortRemoved, void(const device::mojom::SerialPortInfo&));
  MOCK_METHOD1(OnPortConnectedStateChanged,
               void(const device::mojom::SerialPortInfo&));
  MOCK_METHOD0(OnPortManagerConnectionError, void());
  MOCK_METHOD1(OnPermissionRevoked, void(const url::Origin&));
};

device::mojom::SerialPortInfoPtr CreatePersistentBluetoothPort(
    std::optional<std::string> name,
    const std::string& bluetooth_address) {
  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  port->display_name = std::move(name);
  port->path = base::FilePath().AppendASCII(bluetooth_address);
  port->bluetooth_service_class_id =
      device::BluetoothUUID("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa");
  port->has_vendor_id = false;
  port->has_product_id = false;
  return port;
}

device::mojom::SerialPortInfoPtr CreatePersistentUsbPort(
    std::optional<std::string> name,
    const std::string& persistent_id) {
  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  port->display_name = std::move(name);
#if BUILDFLAG(IS_WIN)
  port->device_instance_id = persistent_id;
#else
  port->has_vendor_id = true;
  port->vendor_id = 0;
  port->has_product_id = true;
  port->product_id = 0;
  port->serial_number = persistent_id;
#if BUILDFLAG(IS_MAC)
  port->usb_driver_name = "AppleUSBCDC";
#endif
#endif  // BUILDFLAG(IS_WIN)
  return port;
}

class SerialChooserContextTestBase {
 public:
  SerialChooserContextTestBase() = default;
  ~SerialChooserContextTestBase() = default;

  // Disallow copy and assignment.
  SerialChooserContextTestBase(SerialChooserContextTestBase&) = delete;
  SerialChooserContextTestBase& operator=(SerialChooserContextTestBase&) =
      delete;

  void DoSetUp(bool is_affiliated) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    auto fake_user_manager = std::make_unique<ash::FakeChromeUserManager>();
    auto* fake_user_manager_ptr = fake_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    constexpr char kTestUserGaiaId[] = "1111111111";
    auto account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
    fake_user_manager_ptr->AddUserWithAffiliation(account_id, is_affiliated);
    fake_user_manager_ptr->LoginUser(account_id);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    profile_ = testing_profile_manager_->CreateTestingProfile(kTestUserEmail);
    ASSERT_TRUE(profile_);

    mojo::PendingRemote<device::mojom::SerialPortManager> port_manager;
    port_manager_.AddReceiver(port_manager.InitWithNewPipeAndPassReceiver());

    context_ = SerialChooserContextFactory::GetForProfile(profile_);
    context_->SetPortManagerForTesting(std::move(port_manager));
    scoped_permission_observation_.Observe(context_.get());
    scoped_port_observation_.Observe(context_.get());

    // Ensure |context_| is ready to receive SerialPortManagerClient messages.
    context_->FlushPortManagerConnectionForTesting();
  }

  void DoTearDown() {
    // Because SerialBlocklist is a singleton it must be cleared after tests run
    // to prevent leakage between tests.
    feature_list_.Reset();
    SerialBlocklist::Get().ResetToDefaultValuesForTesting();
  }

  void SetDynamicBlocklist(std::string_view value) {
    feature_list_.Reset();

    std::map<std::string, std::string> parameters;
    parameters[kWebSerialBlocklistAdditions.name] = std::string(value);
    feature_list_.InitWithFeaturesAndParameters(
        {{kWebSerialBlocklist, parameters}}, {});

    SerialBlocklist::Get().ResetToDefaultValuesForTesting();
  }

  device::FakeSerialPortManager& port_manager() { return port_manager_; }
  TestingProfile* profile() { return profile_; }
  TestingPrefServiceSimple* local_state() {
    return testing_profile_manager_->local_state()->Get();
  }
  SerialChooserContext* context() { return context_; }
  permissions::MockPermissionObserver& permission_observer() {
    return permission_observer_;
  }
  NiceMock<MockPortObserver>& port_observer() { return port_observer_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  device::FakeSerialPortManager port_manager_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
#endif

  raw_ptr<SerialChooserContext> context_;
  NiceMock<permissions::MockPermissionObserver> permission_observer_;
  base::ScopedObservation<
      permissions::ObjectPermissionContextBase,
      permissions::ObjectPermissionContextBase::PermissionObserver>
      scoped_permission_observation_{&permission_observer_};
  NiceMock<MockPortObserver> port_observer_;
  base::ScopedObservation<SerialChooserContext,
                          SerialChooserContext::PortObserver>
      scoped_port_observation_{&port_observer_};
};

class SerialChooserContextTest : public SerialChooserContextTestBase,
                                 public testing::Test {
 public:
  void SetUp() override { DoSetUp(/*is_affiliated=*/true); }
  void TearDown() override { DoTearDown(); }
};

class SerialChooserContextAffiliatedTest : public SerialChooserContextTestBase,
                                           public testing::TestWithParam<bool> {
 public:
  SerialChooserContextAffiliatedTest() : is_affiliated_(GetParam()) {}

  void SetUp() override { DoSetUp(is_affiliated_); }
  void TearDown() override { DoTearDown(); }

  bool is_affiliated() const { return is_affiliated_; }

 private:
  bool is_affiliated_;
};

}  // namespace

TEST_F(SerialChooserContextTest, GrantAndRevokeEphemeralPermission) {
  base::HistogramTester histogram_tester;

  const auto origin = url::Origin::Create(GURL("https://google.com"));

  auto port_1 = device::mojom::SerialPortInfo::New();
  port_1->token = base::UnguessableToken::Create();

  auto port_2 = CreatePersistentUsbPort("Persistent Port", "ABC123");
  auto port_3 = CreatePersistentBluetoothPort("Persistent Bluetooth Port",
                                              "aa:aa:aa:aa:aa:aa");

  EXPECT_FALSE(context()->HasPortPermission(origin, *port_1));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_2));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_3));

  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  std::make_optional(ContentSettingsType::SERIAL_GUARD),
                  ContentSettingsType::SERIAL_CHOOSER_DATA));

  context()->GrantPortPermission(origin, *port_1);
  EXPECT_TRUE(context()->HasPortPermission(origin, *port_1));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_2));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_3));

  std::vector<std::unique_ptr<SerialChooserContext::Object>> origin_objects =
      context()->GetGrantedObjects(origin);
  ASSERT_EQ(1u, origin_objects.size());

  std::vector<std::unique_ptr<SerialChooserContext::Object>> objects =
      context()->GetAllGrantedObjects();
  ASSERT_EQ(1u, objects.size());
  EXPECT_EQ(origin.GetURL(), objects[0]->origin);
  EXPECT_EQ(origin_objects[0]->value, objects[0]->value);
  EXPECT_EQ(SettingSource::kUser, objects[0]->source);
  EXPECT_FALSE(objects[0]->incognito);

  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  std::make_optional(ContentSettingsType::SERIAL_GUARD),
                  ContentSettingsType::SERIAL_CHOOSER_DATA));
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin));

  context()->RevokeObjectPermission(origin, objects[0]->value);
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_1));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_2));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_3));
  origin_objects = context()->GetGrantedObjects(origin);
  EXPECT_EQ(0u, origin_objects.size());
  objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(0u, objects.size());

  histogram_tester.ExpectUniqueSample("Permissions.Serial.Revoked",
                                      SerialPermissionRevoked::kEphemeralByUser,
                                      1);
}

TEST_F(SerialChooserContextTest, RevokeEphemeralPermissionByWebsite) {
  base::HistogramTester histogram_tester;

  const auto origin = url::Origin::Create(GURL("https://google.com"));

  auto port_1 = device::mojom::SerialPortInfo::New();
  port_1->token = base::UnguessableToken::Create();

  auto port_2 = CreatePersistentUsbPort("Persistent Port", "ABC123");

  EXPECT_FALSE(context()->HasPortPermission(origin, *port_1));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_2));

  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  std::make_optional(ContentSettingsType::SERIAL_GUARD),
                  ContentSettingsType::SERIAL_CHOOSER_DATA));

  context()->GrantPortPermission(origin, *port_1);
  EXPECT_TRUE(context()->HasPortPermission(origin, *port_1));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_2));

  std::vector<std::unique_ptr<SerialChooserContext::Object>> origin_objects =
      context()->GetGrantedObjects(origin);
  ASSERT_EQ(1u, origin_objects.size());

  std::vector<std::unique_ptr<SerialChooserContext::Object>> objects =
      context()->GetAllGrantedObjects();
  ASSERT_EQ(1u, objects.size());
  EXPECT_EQ(origin.GetURL(), objects[0]->origin);
  EXPECT_EQ(origin_objects[0]->value, objects[0]->value);
  EXPECT_EQ(SettingSource::kUser, objects[0]->source);
  EXPECT_FALSE(objects[0]->incognito);

  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  std::make_optional(ContentSettingsType::SERIAL_GUARD),
                  ContentSettingsType::SERIAL_CHOOSER_DATA));
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin));

  context()->RevokePortPermissionWebInitiated(origin, port_1->token);
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_1));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_2));
  origin_objects = context()->GetGrantedObjects(origin);
  EXPECT_EQ(0u, origin_objects.size());
  objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(0u, objects.size());

  histogram_tester.ExpectUniqueSample(
      "Permissions.Serial.Revoked",
      SerialPermissionRevoked::kEphemeralByWebsite, 1);
}

TEST_F(SerialChooserContextTest, GrantAndCheckPersistentUsbPermission) {
  const auto origin = url::Origin::Create(GURL("https://google.com"));

  // Connect a USB serial port and a Bluetooth serial port. Both are eligible
  // for persistent permissions.
  auto port_1 = CreatePersistentUsbPort("Persistent USB Port", "ABC123");
  auto port_2 = CreatePersistentBluetoothPort("Persistent Bluetooth Port",
                                              "aa:aa:aa:aa:aa:aa");

  EXPECT_FALSE(context()->HasPortPermission(origin, *port_1));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_2));

  // Grant a persistent permission for the USB serial port.
  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  std::make_optional(ContentSettingsType::SERIAL_GUARD),
                  ContentSettingsType::SERIAL_CHOOSER_DATA));
  context()->GrantPortPermission(origin, *port_1);

  // Check that permission was granted only for the USB serial port.
  EXPECT_TRUE(context()->HasPortPermission(origin, *port_1));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_2));
}

TEST_F(SerialChooserContextTest, GrantAndCheckPersistentBluetoothPermission) {
  const auto origin = url::Origin::Create(GURL("https://google.com"));

  // Connect a USB serial port and a Bluetooth serial port. Both are eligible
  // for persistent permissions.
  auto port_1 = CreatePersistentUsbPort("Persistent USB Port", "ABC123");
  auto port_2 = CreatePersistentBluetoothPort("Persistent Bluetooth Port",
                                              "aa:aa:aa:aa:aa:aa");

  EXPECT_FALSE(context()->HasPortPermission(origin, *port_1));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_2));

  // Grant a persistent permission for the Bluetooth serial port.
  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  std::make_optional(ContentSettingsType::SERIAL_GUARD),
                  ContentSettingsType::SERIAL_CHOOSER_DATA));
  context()->GrantPortPermission(origin, *port_2);

  // Check that permission was granted only for the Bluetooth serial port.
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_1));
  EXPECT_TRUE(context()->HasPortPermission(origin, *port_2));
}

TEST_F(SerialChooserContextTest, GrantAndRevokePersistentUsbPermission) {
  base::HistogramTester histogram_tester;

  const auto origin = url::Origin::Create(GURL("https://google.com"));

  device::mojom::SerialPortInfoPtr port_1 =
      CreatePersistentUsbPort("Persistent Port", "ABC123");

  auto port_2 = device::mojom::SerialPortInfo::New();
  port_2->token = base::UnguessableToken::Create();

  EXPECT_FALSE(context()->HasPortPermission(origin, *port_1));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_2));

  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  std::make_optional(ContentSettingsType::SERIAL_GUARD),
                  ContentSettingsType::SERIAL_CHOOSER_DATA));

  context()->GrantPortPermission(origin, *port_1);
  EXPECT_TRUE(context()->HasPortPermission(origin, *port_1));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_2));

  std::vector<std::unique_ptr<SerialChooserContext::Object>> origin_objects =
      context()->GetGrantedObjects(origin);
  ASSERT_EQ(1u, origin_objects.size());

  std::vector<std::unique_ptr<SerialChooserContext::Object>> objects =
      context()->GetAllGrantedObjects();
  ASSERT_EQ(1u, objects.size());
  EXPECT_EQ(origin.GetURL(), objects[0]->origin);
  EXPECT_EQ(origin_objects[0]->value, objects[0]->value);
  EXPECT_EQ(SettingSource::kUser, objects[0]->source);
  EXPECT_FALSE(objects[0]->incognito);

  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  std::make_optional(ContentSettingsType::SERIAL_GUARD),
                  ContentSettingsType::SERIAL_CHOOSER_DATA));
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin));

  context()->RevokeObjectPermission(origin, objects[0]->value);
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_1));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_2));
  origin_objects = context()->GetGrantedObjects(origin);
  EXPECT_EQ(0u, origin_objects.size());
  objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(0u, objects.size());

  histogram_tester.ExpectUniqueSample(
      "Permissions.Serial.Revoked", SerialPermissionRevoked::kPersistentByUser,
      1);
}

TEST_F(SerialChooserContextTest, GrantAndRevokePersistentBluetoothPermission) {
  base::HistogramTester histogram_tester;

  const auto origin = url::Origin::Create(GURL("https://google.com"));

  auto port_1 = CreatePersistentBluetoothPort("Persistent Bluetooth Port",
                                              "aa:aa:aa:aa:aa:aa");

  auto port_2 = device::mojom::SerialPortInfo::New();
  port_2->token = base::UnguessableToken::Create();

  EXPECT_FALSE(context()->HasPortPermission(origin, *port_1));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_2));

  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  std::make_optional(ContentSettingsType::SERIAL_GUARD),
                  ContentSettingsType::SERIAL_CHOOSER_DATA));

  context()->GrantPortPermission(origin, *port_1);
  EXPECT_TRUE(context()->HasPortPermission(origin, *port_1));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_2));

  std::vector<std::unique_ptr<SerialChooserContext::Object>> origin_objects =
      context()->GetGrantedObjects(origin);
  ASSERT_EQ(1u, origin_objects.size());

  std::vector<std::unique_ptr<SerialChooserContext::Object>> objects =
      context()->GetAllGrantedObjects();
  ASSERT_EQ(1u, objects.size());
  EXPECT_EQ(origin.GetURL(), objects[0]->origin);
  EXPECT_EQ(origin_objects[0]->value, objects[0]->value);
  EXPECT_EQ(SettingSource::kUser, objects[0]->source);
  EXPECT_FALSE(objects[0]->incognito);

  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  std::make_optional(ContentSettingsType::SERIAL_GUARD),
                  ContentSettingsType::SERIAL_CHOOSER_DATA));
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin));

  context()->RevokeObjectPermission(origin, objects[0]->value);
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_1));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_2));
  origin_objects = context()->GetGrantedObjects(origin);
  EXPECT_EQ(0u, origin_objects.size());
  objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(0u, objects.size());

  histogram_tester.ExpectUniqueSample(
      "Permissions.Serial.Revoked", SerialPermissionRevoked::kPersistentByUser,
      1);
}

TEST_F(SerialChooserContextTest, RevokePersistentPermissionByWebsite) {
  base::HistogramTester histogram_tester;

  const auto origin = url::Origin::Create(GURL("https://google.com"));

  device::mojom::SerialPortInfoPtr port_1 =
      CreatePersistentUsbPort("Persistent Port", "ABC123");

  auto port_2 = device::mojom::SerialPortInfo::New();
  port_2->token = base::UnguessableToken::Create();

  EXPECT_FALSE(context()->HasPortPermission(origin, *port_1));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_2));

  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  std::make_optional(ContentSettingsType::SERIAL_GUARD),
                  ContentSettingsType::SERIAL_CHOOSER_DATA));

  context()->GrantPortPermission(origin, *port_1);
  EXPECT_TRUE(context()->HasPortPermission(origin, *port_1));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_2));

  std::vector<std::unique_ptr<SerialChooserContext::Object>> origin_objects =
      context()->GetGrantedObjects(origin);
  ASSERT_EQ(1u, origin_objects.size());

  std::vector<std::unique_ptr<SerialChooserContext::Object>> objects =
      context()->GetAllGrantedObjects();
  ASSERT_EQ(1u, objects.size());
  EXPECT_EQ(origin.GetURL(), objects[0]->origin);
  EXPECT_EQ(origin_objects[0]->value, objects[0]->value);
  EXPECT_EQ(SettingSource::kUser, objects[0]->source);
  EXPECT_FALSE(objects[0]->incognito);

  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  std::make_optional(ContentSettingsType::SERIAL_GUARD),
                  ContentSettingsType::SERIAL_CHOOSER_DATA));
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin));

  context()->RevokePortPermissionWebInitiated(origin, port_1->token);
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_1));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_2));
  origin_objects = context()->GetGrantedObjects(origin);
  EXPECT_EQ(0u, origin_objects.size());
  objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(0u, objects.size());

  histogram_tester.ExpectUniqueSample(
      "Permissions.Serial.Revoked",
      SerialPermissionRevoked::kPersistentByWebsite, 1);
}

TEST_F(SerialChooserContextTest, EphemeralPermissionRevokedOnDisconnect) {
  base::HistogramTester histogram_tester;

  const auto origin = url::Origin::Create(GURL("https://google.com"));

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  port_manager().AddPort(port.Clone());

  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  std::make_optional(ContentSettingsType::SERIAL_GUARD),
                  ContentSettingsType::SERIAL_CHOOSER_DATA))
      .Times(2);
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin));

  context()->GrantPortPermission(origin, *port);
  EXPECT_TRUE(context()->HasPortPermission(origin, *port));

  port_manager().RemovePort(port->token);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(port_observer(), OnPortRemoved(testing::_))
        .WillOnce(
            testing::Invoke([&](const device::mojom::SerialPortInfo& info) {
              EXPECT_EQ(port->token, info.token);
              EXPECT_TRUE(context()->HasPortPermission(origin, info));
              run_loop.Quit();
            }));
    run_loop.Run();
  }
  EXPECT_FALSE(context()->HasPortPermission(origin, *port));
  auto origin_objects = context()->GetGrantedObjects(origin);
  EXPECT_EQ(0u, origin_objects.size());
  auto objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(0u, objects.size());

  histogram_tester.ExpectUniqueSample(
      "Permissions.Serial.Revoked",
      SerialPermissionRevoked::kEphemeralByDisconnect, 1);
}

TEST_F(SerialChooserContextTest, PersistenceRequiresDisplayName) {
  const auto origin = url::Origin::Create(GURL("https://google.com"));

  device::mojom::SerialPortInfoPtr port =
      CreatePersistentUsbPort(/*name=*/std::nullopt, "ABC123");
  port_manager().AddPort(port.Clone());

  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  std::make_optional(ContentSettingsType::SERIAL_GUARD),
                  ContentSettingsType::SERIAL_CHOOSER_DATA))
      .Times(2);
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin));

  context()->GrantPortPermission(origin, *port);
  EXPECT_TRUE(context()->HasPortPermission(origin, *port));

  // Without a display name a persistent permission cannot be recorded and so
  // removing the device will revoke permission.
  port_manager().RemovePort(port->token);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(port_observer(), OnPortRemoved(testing::_))
        .WillOnce(
            testing::Invoke([&](const device::mojom::SerialPortInfo& info) {
              EXPECT_EQ(port->token, info.token);
              EXPECT_TRUE(context()->HasPortPermission(origin, info));
              run_loop.Quit();
            }));
    run_loop.Run();
  }
  EXPECT_FALSE(context()->HasPortPermission(origin, *port));
  auto origin_objects = context()->GetGrantedObjects(origin);
  EXPECT_EQ(0u, origin_objects.size());
  auto objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(0u, objects.size());
}

TEST_F(SerialChooserContextTest, PersistentPermissionNotRevokedOnDisconnect) {
  const auto origin = url::Origin::Create(GURL("https://google.com"));
  const char persistent_id[] = "ABC123";

  device::mojom::SerialPortInfoPtr port =
      CreatePersistentUsbPort("Persistent Port", persistent_id);
  port_manager().AddPort(port.Clone());

  context()->GrantPortPermission(origin, *port);
  EXPECT_TRUE(context()->HasPortPermission(origin, *port));

  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  std::make_optional(ContentSettingsType::SERIAL_GUARD),
                  ContentSettingsType::SERIAL_CHOOSER_DATA))
      .Times(0);
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin)).Times(0);

  port_manager().RemovePort(port->token);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(port_observer(), OnPortRemoved(testing::_))
        .WillOnce(
            testing::Invoke([&](const device::mojom::SerialPortInfo& info) {
              EXPECT_EQ(port->token, info.token);
              EXPECT_TRUE(context()->HasPortPermission(origin, info));
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  EXPECT_TRUE(context()->HasPortPermission(origin, *port));
  auto origin_objects = context()->GetGrantedObjects(origin);
  EXPECT_EQ(1u, origin_objects.size());
  auto objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(1u, objects.size());

  // Simulate reconnection of the port. It gets a new token but the same
  // persistent ID. This SerialPortInfo should still match the old permission.
  port = CreatePersistentUsbPort("Persistent Port", persistent_id);
  port_manager().AddPort(port.Clone());

  EXPECT_TRUE(context()->HasPortPermission(origin, *port));
}

TEST_F(SerialChooserContextTest, GuardPermission) {
  const auto origin = url::Origin::Create(GURL("https://google.com"));

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();

  context()->GrantPortPermission(origin, *port);
  EXPECT_TRUE(context()->HasPortPermission(origin, *port));

  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(origin.GetURL(), origin.GetURL(),
                                     ContentSettingsType::SERIAL_GUARD,
                                     CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(context()->HasPortPermission(origin, *port));

  std::vector<std::unique_ptr<SerialChooserContext::Object>> objects =
      context()->GetGrantedObjects(origin);
  EXPECT_EQ(0u, objects.size());

  std::vector<std::unique_ptr<SerialChooserContext::Object>>
      all_origin_objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(0u, all_origin_objects.size());
}

TEST_F(SerialChooserContextTest, PolicyGuardPermission) {
  const auto origin = url::Origin::Create(GURL("https://google.com"));
  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();

  context()->GrantPortPermission(origin, *port);

  auto* profile_prefs = profile()->GetTestingPrefService();
  profile_prefs->SetManagedPref(
      prefs::kManagedDefaultSerialGuardSetting,
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  EXPECT_FALSE(context()->CanRequestObjectPermission(origin));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port));

  std::vector<std::unique_ptr<SerialChooserContext::Object>> objects =
      context()->GetGrantedObjects(origin);
  EXPECT_EQ(0u, objects.size());

  std::vector<std::unique_ptr<SerialChooserContext::Object>>
      all_origin_objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(0u, all_origin_objects.size());
}

TEST_F(SerialChooserContextTest, PolicyAskForUrls) {
  const auto kFooOrigin = url::Origin::Create(GURL("https://foo.origin"));
  const auto kBarOrigin = url::Origin::Create(GURL("https://bar.origin"));

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  context()->GrantPortPermission(kFooOrigin, *port);
  context()->GrantPortPermission(kBarOrigin, *port);

  // Set the default to "ask" so that the policy being tested overrides it.
  auto* profile_prefs = profile()->GetTestingPrefService();
  profile_prefs->SetManagedPref(
      prefs::kManagedDefaultSerialGuardSetting,
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  profile_prefs->SetManagedPref(prefs::kManagedSerialAskForUrls,
                                ParseJson(R"([ "https://foo.origin" ])"));

  EXPECT_TRUE(context()->CanRequestObjectPermission(kFooOrigin));
  EXPECT_TRUE(context()->HasPortPermission(kFooOrigin, *port));
  EXPECT_FALSE(context()->CanRequestObjectPermission(kBarOrigin));
  EXPECT_FALSE(context()->HasPortPermission(kBarOrigin, *port));

  std::vector<std::unique_ptr<SerialChooserContext::Object>> objects =
      context()->GetGrantedObjects(kFooOrigin);
  EXPECT_EQ(1u, objects.size());
  objects = context()->GetGrantedObjects(kBarOrigin);
  EXPECT_EQ(0u, objects.size());

  std::vector<std::unique_ptr<SerialChooserContext::Object>>
      all_origin_objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());
}

TEST_F(SerialChooserContextTest, PolicyBlockedForUrls) {
  const auto kFooOrigin = url::Origin::Create(GURL("https://foo.origin"));
  const auto kBarOrigin = url::Origin::Create(GURL("https://bar.origin"));

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  context()->GrantPortPermission(kFooOrigin, *port);
  context()->GrantPortPermission(kBarOrigin, *port);

  auto* profile_prefs = profile()->GetTestingPrefService();
  profile_prefs->SetManagedPref(prefs::kManagedSerialBlockedForUrls,
                                ParseJson(R"([ "https://foo.origin" ])"));

  EXPECT_FALSE(context()->CanRequestObjectPermission(kFooOrigin));
  EXPECT_FALSE(context()->HasPortPermission(kFooOrigin, *port));
  EXPECT_TRUE(context()->CanRequestObjectPermission(kBarOrigin));
  EXPECT_TRUE(context()->HasPortPermission(kBarOrigin, *port));

  std::vector<std::unique_ptr<SerialChooserContext::Object>> objects =
      context()->GetGrantedObjects(kFooOrigin);
  EXPECT_EQ(0u, objects.size());
  objects = context()->GetGrantedObjects(kBarOrigin);
  EXPECT_EQ(1u, objects.size());

  std::vector<std::unique_ptr<SerialChooserContext::Object>>
      all_origin_objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());
}

TEST_F(SerialChooserContextTest, BluetoothPortConnectedState) {
  const auto origin = url::Origin::Create(GURL("https://google.com"));

  // Create a Bluetooth serial port.
  auto port = CreatePersistentBluetoothPort("Persistent Bluetooth Port",
                                            "aa:aa:aa:aa:aa:aa");

  // Simulate disconnection of the Bluetooth device hosting `port`. The observer
  // is notified that the port is now disconnected.
  TestFuture<const device::mojom::SerialPortInfo&> port_future;
  EXPECT_CALL(port_observer(), OnPortConnectedStateChanged)
      .WillOnce(InvokeFuture(port_future));
  auto disconnected_port = port.Clone();
  disconnected_port->connected = false;
  context()->OnPortConnectedStateChanged(std::move(disconnected_port));
  EXPECT_EQ(port_future.Get().token, port->token);
  EXPECT_FALSE(port_future.Get().connected);
}

TEST_P(SerialChooserContextAffiliatedTest, PolicyAllowForUrls) {
  const auto kFooOrigin = url::Origin::Create(GURL("https://foo.origin"));
  const auto kBarOrigin = url::Origin::Create(GURL("https://bar.origin"));

  local_state()->SetManagedPref(prefs::kManagedSerialAllowAllPortsForUrls,
                                ParseJson(R"([ "https://foo.origin" ])"));
  local_state()->SetManagedPref(prefs::kManagedSerialAllowUsbDevicesForUrls,
                                ParseJson(R"([
               {
                 "devices": [{ "vendor_id": 6353, "product_id": 19985 }],
                 "urls": [ "https://bar.origin" ]
               }
             ])"));

  auto platform_port = device::mojom::SerialPortInfo::New();
  platform_port->token = base::UnguessableToken::Create();

  auto usb_port1 = device::mojom::SerialPortInfo::New();
  usb_port1->token = base::UnguessableToken::Create();
  usb_port1->has_vendor_id = true;
  usb_port1->vendor_id = 0x18D1;
  usb_port1->has_product_id = true;
  usb_port1->product_id = 0x4E11;

  auto usb_port2 = device::mojom::SerialPortInfo::New();
  usb_port2->token = base::UnguessableToken::Create();
  usb_port2->has_vendor_id = true;
  usb_port2->vendor_id = 0x18D1;
  usb_port2->has_product_id = true;
  usb_port2->product_id = 0x4E12;

  EXPECT_TRUE(context()->CanRequestObjectPermission(kFooOrigin));
  EXPECT_TRUE(context()->CanRequestObjectPermission(kBarOrigin));

  if (is_affiliated()) {
    EXPECT_TRUE(context()->HasPortPermission(kFooOrigin, *platform_port));
    EXPECT_TRUE(context()->HasPortPermission(kFooOrigin, *usb_port1));
    EXPECT_TRUE(context()->HasPortPermission(kFooOrigin, *usb_port2));

    EXPECT_FALSE(context()->HasPortPermission(kBarOrigin, *platform_port));
    EXPECT_TRUE(context()->HasPortPermission(kBarOrigin, *usb_port1));
    EXPECT_FALSE(context()->HasPortPermission(kBarOrigin, *usb_port2));

    auto foo_objects = context()->GetGrantedObjects(kFooOrigin);
    ASSERT_EQ(1u, foo_objects.size());
    const auto& foo_object = foo_objects.front();
    EXPECT_EQ(kFooOrigin.GetURL(), foo_object->origin);
    EXPECT_EQ(u"Any serial port",
              context()->GetObjectDisplayName(foo_object->value));
    EXPECT_EQ(SettingSource::kPolicy, foo_object->source);
    EXPECT_FALSE(foo_object->incognito);

    auto bar_objects = context()->GetGrantedObjects(kBarOrigin);
    ASSERT_EQ(1u, bar_objects.size());
    const auto& bar_object = bar_objects.front();
    EXPECT_EQ(kBarOrigin.GetURL(), bar_object->origin);
    EXPECT_EQ(u"Nexus One", context()->GetObjectDisplayName(bar_object->value));
    EXPECT_EQ(SettingSource::kPolicy, bar_object->source);
    EXPECT_FALSE(bar_object->incognito);

    auto all_objects = context()->GetAllGrantedObjects();
    EXPECT_EQ(2u, all_objects.size());
    bool found_foo_object = false, found_bar_object = false;
    for (const auto& object : all_objects) {
      if (object->origin == kFooOrigin.GetURL()) {
        EXPECT_FALSE(found_foo_object);
        found_foo_object = true;
        EXPECT_EQ(u"Any serial port",
                  context()->GetObjectDisplayName(object->value));
      } else if (object->origin == kBarOrigin.GetURL()) {
        EXPECT_FALSE(found_bar_object);
        found_bar_object = true;
        EXPECT_EQ(u"Nexus One", context()->GetObjectDisplayName(object->value));
      }
      EXPECT_EQ(SettingSource::kPolicy, object->source);
      EXPECT_FALSE(object->incognito);
    }
    EXPECT_TRUE(found_foo_object);
    EXPECT_TRUE(found_bar_object);
  } else {
    // Policy-defined port permissions are not set for unaffiliated users.
    EXPECT_FALSE(context()->HasPortPermission(kFooOrigin, *platform_port));
    EXPECT_FALSE(context()->HasPortPermission(kFooOrigin, *usb_port1));
    EXPECT_FALSE(context()->HasPortPermission(kFooOrigin, *usb_port2));

    EXPECT_FALSE(context()->HasPortPermission(kBarOrigin, *platform_port));
    EXPECT_FALSE(context()->HasPortPermission(kBarOrigin, *usb_port1));
    EXPECT_FALSE(context()->HasPortPermission(kBarOrigin, *usb_port2));

    auto foo_objects = context()->GetGrantedObjects(kFooOrigin);
    EXPECT_EQ(0u, foo_objects.size());

    auto bar_objects = context()->GetGrantedObjects(kBarOrigin);
    EXPECT_EQ(0u, bar_objects.size());

    auto all_objects = context()->GetAllGrantedObjects();
    EXPECT_EQ(0u, all_objects.size());
  }
}

TEST_P(SerialChooserContextAffiliatedTest,
       PolicyAllowForUrlsDescriptionStrings) {
  const auto kFooOrigin = url::Origin::Create(GURL("https://foo.origin"));
  const auto kBarOrigin = url::Origin::Create(GURL("https://bar.origin"));

  local_state()->SetManagedPref(prefs::kManagedSerialAllowUsbDevicesForUrls,
                                ParseJson(R"([
               {
                 "devices": [{ "vendor_id": 6353 }],
                 "urls": [ "https://google.com" ]
               },
               {
                 "devices": [{ "vendor_id": 6354 }],
                 "urls": [ "https://unknown-vendor.com" ]
               },
               {
                 "devices": [{ "vendor_id": 6353, "product_id": 5678 }],
                 "urls": [ "https://unknown-product.google.com" ]
               },
               {
                 "devices": [{ "vendor_id": 6354, "product_id": 5678 }],
                 "urls": [ "https://unknown-product.unknown-vendor.com" ]
               }
             ])"));

  if (is_affiliated()) {
    auto google_objects = context()->GetGrantedObjects(
        url::Origin::Create(GURL("https://google.com")));
    ASSERT_EQ(1u, google_objects.size());
    EXPECT_EQ(u"USB devices from Google Inc.",
              context()->GetObjectDisplayName(google_objects[0]->value));

    auto unknown_vendor_objects = context()->GetGrantedObjects(
        url::Origin::Create(GURL("https://unknown-vendor.com")));
    ASSERT_EQ(1u, unknown_vendor_objects.size());
    EXPECT_EQ(
        u"USB devices from vendor 18D2",
        context()->GetObjectDisplayName(unknown_vendor_objects[0]->value));

    auto unknown_product_objects = context()->GetGrantedObjects(
        url::Origin::Create(GURL("https://unknown-product.google.com")));
    ASSERT_EQ(1u, unknown_product_objects.size());
    EXPECT_EQ(
        u"USB device from Google Inc. (product 162E)",
        context()->GetObjectDisplayName(unknown_product_objects[0]->value));

    auto unknown_product_and_vendor_objects =
        context()->GetGrantedObjects(url::Origin::Create(
            GURL("https://unknown-product.unknown-vendor.com")));
    ASSERT_EQ(1u, unknown_product_and_vendor_objects.size());
    EXPECT_EQ(u"USB device (18D2:162E)",
              context()->GetObjectDisplayName(
                  unknown_product_and_vendor_objects[0]->value));
  } else {
    // Policy-defined port permissions are not set for unaffiliated users.
    auto google_objects = context()->GetGrantedObjects(
        url::Origin::Create(GURL("https://google.com")));
    EXPECT_EQ(0u, google_objects.size());

    auto unknown_vendor_objects = context()->GetGrantedObjects(
        url::Origin::Create(GURL("https://unknown-vendor.com")));
    EXPECT_EQ(0u, unknown_vendor_objects.size());

    auto unknown_product_objects = context()->GetGrantedObjects(
        url::Origin::Create(GURL("https://unknown-product.google.com")));
    EXPECT_EQ(0u, unknown_product_objects.size());

    auto unknown_product_and_vendor_objects =
        context()->GetGrantedObjects(url::Origin::Create(
            GURL("https://unknown-product.unknown-vendor.com")));
    EXPECT_EQ(0u, unknown_product_and_vendor_objects.size());
  }
}

TEST_P(SerialChooserContextAffiliatedTest, PolicyAllowOverridesGuard) {
  const auto kFooOrigin = url::Origin::Create(GURL("https://foo.origin"));
  const auto kBarOrigin = url::Origin::Create(GURL("https://bar.origin"));

  auto* profile_prefs = profile()->GetTestingPrefService();
  profile_prefs->SetManagedPref(
      prefs::kManagedDefaultSerialGuardSetting,
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  local_state()->SetManagedPref(prefs::kManagedSerialAllowAllPortsForUrls,
                                ParseJson(R"([ "https://foo.origin" ])"));

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();

  EXPECT_FALSE(context()->CanRequestObjectPermission(kFooOrigin));

  if (is_affiliated()) {
    EXPECT_TRUE(context()->HasPortPermission(kFooOrigin, *port));
  } else {
    // Policy-defined port permissions are not set for unaffiliated users.
    EXPECT_FALSE(context()->HasPortPermission(kFooOrigin, *port));
  }

  EXPECT_FALSE(context()->CanRequestObjectPermission(kBarOrigin));
  EXPECT_FALSE(context()->HasPortPermission(kBarOrigin, *port));
}

TEST_P(SerialChooserContextAffiliatedTest, PolicyAllowOverridesBlocked) {
  const auto kFooOrigin = url::Origin::Create(GURL("https://foo.origin"));
  const auto kBarOrigin = url::Origin::Create(GURL("https://bar.origin"));

  auto* profile_prefs = profile()->GetTestingPrefService();
  profile_prefs->SetManagedPref(
      prefs::kManagedSerialBlockedForUrls,
      ParseJson(R"([ "https://foo.origin", "https://bar.origin" ])"));
  local_state()->SetManagedPref(prefs::kManagedSerialAllowAllPortsForUrls,
                                ParseJson(R"([ "https://foo.origin" ])"));

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();

  EXPECT_FALSE(context()->CanRequestObjectPermission(kFooOrigin));

  if (is_affiliated()) {
    EXPECT_TRUE(context()->HasPortPermission(kFooOrigin, *port));
  } else {
    // Policy-defined port permissions are not set for unaffiliated users.
    EXPECT_FALSE(context()->HasPortPermission(kFooOrigin, *port));
  }

  EXPECT_FALSE(context()->CanRequestObjectPermission(kBarOrigin));
  EXPECT_FALSE(context()->HasPortPermission(kBarOrigin, *port));
}

TEST_F(SerialChooserContextTest, Blocklist) {
  const auto origin = url::Origin::Create(GURL("https://google.com"));

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  port->has_vendor_id = true;
  port->vendor_id = 0x18D1;
  port->has_product_id = true;
  port->product_id = 0x58F0;
  context()->GrantPortPermission(origin, *port);
  EXPECT_TRUE(context()->HasPortPermission(origin, *port));

  // Adding a USB device to the blocklist overrides any previously granted
  // permissions.
  SetDynamicBlocklist("usb:18D1:58F0");
  EXPECT_FALSE(context()->HasPortPermission(origin, *port));

  // The lists of granted permissions will still include the entry because
  // permission storage does not include the USB vendor and product IDs on all
  // platforms and users should still be made aware of permissions they've
  // granted even if they are being blocked from taking effect.
  std::vector<std::unique_ptr<SerialChooserContext::Object>> objects =
      context()->GetGrantedObjects(origin);
  EXPECT_EQ(1u, objects.size());

  std::vector<std::unique_ptr<SerialChooserContext::Object>>
      all_origin_objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());
}

TEST_P(SerialChooserContextAffiliatedTest, BlocklistOverridesPolicy) {
  const auto origin = url::Origin::Create(GURL("https://google.com"));

  local_state()->SetManagedPref(prefs::kManagedSerialAllowUsbDevicesForUrls,
                                ParseJson(R"([
               {
                 "devices": [{ "vendor_id": 6353, "product_id": 22768 }],
                 "urls": [ "https://google.com" ]
               }
             ])"));

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  port->has_vendor_id = true;
  port->vendor_id = 0x18D1;
  port->has_product_id = true;
  port->product_id = 0x58F0;

  if (is_affiliated()) {
    EXPECT_TRUE(context()->HasPortPermission(origin, *port));
  } else {
    // Policy-defined port permissions are not set for unaffiliated users.
    EXPECT_FALSE(context()->HasPortPermission(origin, *port));
  }

  // Adding a USB device to the blocklist overrides permissions granted by
  // policy.
  SetDynamicBlocklist("usb:18D1:58F0");
  EXPECT_FALSE(context()->HasPortPermission(origin, *port));
}

// Boolean parameter means if user is affiliated on the device. Affiliated
// users belong to the domain that owns the device and is only meaningful
// on Chrome OS.
//
// The SerialAllowAllPortsForUrls and SerialAllowUsbDevicesForUrls policies
// only take effect for sffiliated users.
INSTANTIATE_TEST_SUITE_P(
    SerialChooserContextAffiliatedTestInstance,
    SerialChooserContextAffiliatedTest,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    testing::Values(true, false),
#else
    testing::Values(true),
#endif
    [](const testing::TestParamInfo<
        SerialChooserContextAffiliatedTest::ParamType>& info) {
      return info.param ? "affiliated" : "unaffiliated";
    });
