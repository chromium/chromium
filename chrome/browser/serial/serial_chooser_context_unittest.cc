// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/serial_chooser_context.h"

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/serial/serial_blocklist.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/serial/serial_chooser_histograms.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/test/chooser_context_base_mock_permission_observer.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "services/device/public/cpp/test/fake_serial_port_manager.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockPortObserver : public SerialChooserContext::PortObserver {
 public:
  MockPortObserver() = default;
  MockPortObserver(MockPortObserver&) = delete;
  MockPortObserver& operator=(MockPortObserver&) = delete;
  ~MockPortObserver() override = default;

  MOCK_METHOD1(OnPortAdded, void(const device::mojom::SerialPortInfo&));
  MOCK_METHOD1(OnPortRemoved, void(const device::mojom::SerialPortInfo&));
  MOCK_METHOD0(OnPortManagerConnectionError, void());
};

device::mojom::SerialPortInfoPtr CreatePersistentPort(
    base::Optional<std::string> name,
    const std::string& persistent_id) {
  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  port->display_name = std::move(name);
#if defined(OS_WIN)
  port->device_instance_id = persistent_id;
#else
  port->has_vendor_id = true;
  port->vendor_id = 0;
  port->has_product_id = true;
  port->product_id = 0;
  port->serial_number = persistent_id;
#if defined(OS_MAC)
  port->usb_driver_name = "AppleUSBCDC";
#endif
#endif  // defined(OS_WIN)
  return port;
}

std::unique_ptr<base::Value> ReadJson(base::StringPiece json) {
  base::JSONReader::ValueWithError result =
      base::JSONReader::ReadAndReturnValueWithError(json);
  EXPECT_TRUE(result.value) << result.error_message;
  return result.value ? base::Value::ToUniquePtrValue(std::move(*result.value))
                      : nullptr;
}

class SerialChooserContextTest : public testing::Test {
 public:
  SerialChooserContextTest() {
    mojo::PendingRemote<device::mojom::SerialPortManager> port_manager;
    port_manager_.AddReceiver(port_manager.InitWithNewPipeAndPassReceiver());

    context_ = SerialChooserContextFactory::GetForProfile(&profile_);
    context_->SetPortManagerForTesting(std::move(port_manager));
    scoped_permission_observer_.Add(context_);
    scoped_port_observer_.Add(context_);

    // Ensure |context_| is ready to receive SerialPortManagerClient messages.
    context_->FlushPortManagerConnectionForTesting();
  }

  ~SerialChooserContextTest() override = default;

  // Disallow copy and assignment.
  SerialChooserContextTest(SerialChooserContextTest&) = delete;
  SerialChooserContextTest& operator=(SerialChooserContextTest&) = delete;

  void TearDown() override {
    // Because SerialBlocklist is a singleton it must be cleared after tests run
    // to prevent leakage between tests.
    feature_list_.Reset();
    SerialBlocklist::Get().ResetToDefaultValuesForTesting();
  }

  void SetDynamicBlocklist(base::StringPiece value) {
    feature_list_.Reset();

    std::map<std::string, std::string> parameters;
    parameters[kWebSerialBlocklistAdditions.name] = std::string(value);
    feature_list_.InitWithFeaturesAndParameters(
        {{kWebSerialBlocklist, parameters}}, {});

    SerialBlocklist::Get().ResetToDefaultValuesForTesting();
  }

  device::FakeSerialPortManager& port_manager() { return port_manager_; }
  TestingProfile* profile() { return &profile_; }
  SerialChooserContext* context() { return context_; }
  permissions::MockPermissionObserver& permission_observer() {
    return permission_observer_;
  }
  MockPortObserver& port_observer() { return port_observer_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  device::FakeSerialPortManager port_manager_;
  TestingProfile profile_;
  SerialChooserContext* context_;
  permissions::MockPermissionObserver permission_observer_;
  ScopedObserver<permissions::ChooserContextBase,
                 permissions::ChooserContextBase::PermissionObserver>
      scoped_permission_observer_{&permission_observer_};
  MockPortObserver port_observer_;
  ScopedObserver<SerialChooserContext,
                 SerialChooserContext::PortObserver,
                 &SerialChooserContext::AddPortObserver,
                 &SerialChooserContext::RemovePortObserver>
      scoped_port_observer_{&port_observer_};
};

}  // namespace

TEST_F(SerialChooserContextTest, GrantAndRevokeEphemeralPermission) {
  base::HistogramTester histogram_tester;

  const auto origin = url::Origin::Create(GURL("https://google.com"));

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();

  EXPECT_FALSE(context()->HasPortPermission(origin, *port));

  EXPECT_CALL(permission_observer(),
              OnChooserObjectPermissionChanged(
                  ContentSettingsType::SERIAL_GUARD,
                  ContentSettingsType::SERIAL_CHOOSER_DATA));

  context()->GrantPortPermission(origin, *port);
  EXPECT_TRUE(context()->HasPortPermission(origin, *port));

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      origin_objects = context()->GetGrantedObjects(origin);
  ASSERT_EQ(1u, origin_objects.size());

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = context()->GetAllGrantedObjects();
  ASSERT_EQ(1u, objects.size());
  EXPECT_EQ(origin.GetURL(), objects[0]->origin);
  EXPECT_EQ(origin_objects[0]->value, objects[0]->value);
  EXPECT_EQ(content_settings::SettingSource::SETTING_SOURCE_USER,
            objects[0]->source);
  EXPECT_FALSE(objects[0]->incognito);

  EXPECT_CALL(permission_observer(),
              OnChooserObjectPermissionChanged(
                  ContentSettingsType::SERIAL_GUARD,
                  ContentSettingsType::SERIAL_CHOOSER_DATA));
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin));

  context()->RevokeObjectPermission(origin, objects[0]->value);
  EXPECT_FALSE(context()->HasPortPermission(origin, *port));
  origin_objects = context()->GetGrantedObjects(origin);
  EXPECT_EQ(0u, origin_objects.size());
  objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(0u, objects.size());

  histogram_tester.ExpectUniqueSample("Permissions.Serial.Revoked",
                                      SerialPermissionRevoked::kEphemeralByUser,
                                      1);
}

TEST_F(SerialChooserContextTest, GrantAndRevokePersistentPermission) {
  base::HistogramTester histogram_tester;

  const auto origin = url::Origin::Create(GURL("https://google.com"));

  device::mojom::SerialPortInfoPtr port =
      CreatePersistentPort("Persistent Port", "ABC123");

  EXPECT_FALSE(context()->HasPortPermission(origin, *port));

  EXPECT_CALL(permission_observer(),
              OnChooserObjectPermissionChanged(
                  ContentSettingsType::SERIAL_GUARD,
                  ContentSettingsType::SERIAL_CHOOSER_DATA));

  context()->GrantPortPermission(origin, *port);
  EXPECT_TRUE(context()->HasPortPermission(origin, *port));

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      origin_objects = context()->GetGrantedObjects(origin);
  ASSERT_EQ(1u, origin_objects.size());

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = context()->GetAllGrantedObjects();
  ASSERT_EQ(1u, objects.size());
  EXPECT_EQ(origin.GetURL(), objects[0]->origin);
  EXPECT_EQ(origin_objects[0]->value, objects[0]->value);
  EXPECT_EQ(content_settings::SettingSource::SETTING_SOURCE_USER,
            objects[0]->source);
  EXPECT_FALSE(objects[0]->incognito);

  EXPECT_CALL(permission_observer(),
              OnChooserObjectPermissionChanged(
                  ContentSettingsType::SERIAL_GUARD,
                  ContentSettingsType::SERIAL_CHOOSER_DATA));
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin));

  context()->RevokeObjectPermission(origin, objects[0]->value);
  EXPECT_FALSE(context()->HasPortPermission(origin, *port));
  origin_objects = context()->GetGrantedObjects(origin);
  EXPECT_EQ(0u, origin_objects.size());
  objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(0u, objects.size());

  histogram_tester.ExpectUniqueSample("Permissions.Serial.Revoked",
                                      SerialPermissionRevoked::kPersistent, 1);
}

TEST_F(SerialChooserContextTest, EphemeralPermissionRevokedOnDisconnect) {
  base::HistogramTester histogram_tester;

  const auto origin = url::Origin::Create(GURL("https://google.com"));

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  port_manager().AddPort(port.Clone());

  context()->GrantPortPermission(origin, *port);
  EXPECT_TRUE(context()->HasPortPermission(origin, *port));

  EXPECT_CALL(permission_observer(),
              OnChooserObjectPermissionChanged(
                  ContentSettingsType::SERIAL_GUARD,
                  ContentSettingsType::SERIAL_CHOOSER_DATA));
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin));

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
      CreatePersistentPort(/*name=*/base::nullopt, "ABC123");
  port_manager().AddPort(port.Clone());

  context()->GrantPortPermission(origin, *port);
  EXPECT_TRUE(context()->HasPortPermission(origin, *port));

  EXPECT_CALL(permission_observer(),
              OnChooserObjectPermissionChanged(
                  ContentSettingsType::SERIAL_GUARD,
                  ContentSettingsType::SERIAL_CHOOSER_DATA));
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin));

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
      CreatePersistentPort("Persistent Port", persistent_id);
  port_manager().AddPort(port.Clone());

  context()->GrantPortPermission(origin, *port);
  EXPECT_TRUE(context()->HasPortPermission(origin, *port));

  EXPECT_CALL(permission_observer(),
              OnChooserObjectPermissionChanged(
                  ContentSettingsType::SERIAL_GUARD,
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
  port = CreatePersistentPort("Persistent Port", persistent_id);
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

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = context()->GetGrantedObjects(origin);
  EXPECT_EQ(0u, objects.size());

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      all_origin_objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(0u, all_origin_objects.size());
}

TEST_F(SerialChooserContextTest, PolicyGuardPermission) {
  const auto origin = url::Origin::Create(GURL("https://google.com"));

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  context()->GrantPortPermission(origin, *port);

  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultSerialGuardSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  EXPECT_FALSE(context()->CanRequestObjectPermission(origin));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port));

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = context()->GetGrantedObjects(origin);
  EXPECT_EQ(0u, objects.size());

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
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
  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultSerialGuardSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  prefs->SetManagedPref(prefs::kManagedSerialAskForUrls,
                        ReadJson(R"([ "https://foo.origin" ])"));

  EXPECT_TRUE(context()->CanRequestObjectPermission(kFooOrigin));
  EXPECT_TRUE(context()->HasPortPermission(kFooOrigin, *port));
  EXPECT_FALSE(context()->CanRequestObjectPermission(kBarOrigin));
  EXPECT_FALSE(context()->HasPortPermission(kBarOrigin, *port));

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = context()->GetGrantedObjects(kFooOrigin);
  EXPECT_EQ(1u, objects.size());
  objects = context()->GetGrantedObjects(kBarOrigin);
  EXPECT_EQ(0u, objects.size());

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
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

  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedSerialBlockedForUrls,
                        ReadJson(R"([ "https://foo.origin" ])"));

  EXPECT_FALSE(context()->CanRequestObjectPermission(kFooOrigin));
  EXPECT_FALSE(context()->HasPortPermission(kFooOrigin, *port));
  EXPECT_TRUE(context()->CanRequestObjectPermission(kBarOrigin));
  EXPECT_TRUE(context()->HasPortPermission(kBarOrigin, *port));

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = context()->GetGrantedObjects(kFooOrigin);
  EXPECT_EQ(0u, objects.size());
  objects = context()->GetGrantedObjects(kBarOrigin);
  EXPECT_EQ(1u, objects.size());

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      all_origin_objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());
}

TEST_F(SerialChooserContextTest, PolicyAllowForUrls) {
  const auto kFooOrigin = url::Origin::Create(GURL("https://foo.origin"));
  const auto kBarOrigin = url::Origin::Create(GURL("https://bar.origin"));

  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedSerialAllowAllPortsForUrls,
                        ReadJson(R"([ "https://foo.origin" ])"));
  prefs->SetManagedPref(prefs::kManagedSerialAllowUsbDevicesForUrls,
                        ReadJson(R"([
               {
                 "devices": [{ "vendor_id": 1234, "product_id": 5678 }],
                 "urls": [ "https://bar.origin" ]
               }
             ])"));

  auto platform_port = device::mojom::SerialPortInfo::New();
  platform_port->token = base::UnguessableToken::Create();

  auto usb_port1 = device::mojom::SerialPortInfo::New();
  usb_port1->token = base::UnguessableToken::Create();
  usb_port1->has_vendor_id = true;
  usb_port1->vendor_id = 1234;
  usb_port1->has_product_id = true;
  usb_port1->product_id = 5678;

  auto usb_port2 = device::mojom::SerialPortInfo::New();
  usb_port2->token = base::UnguessableToken::Create();
  usb_port2->has_vendor_id = true;
  usb_port2->vendor_id = 1234;
  usb_port2->has_product_id = true;
  usb_port2->product_id = 8765;

  EXPECT_TRUE(context()->CanRequestObjectPermission(kFooOrigin));
  EXPECT_TRUE(context()->HasPortPermission(kFooOrigin, *platform_port));
  EXPECT_TRUE(context()->HasPortPermission(kFooOrigin, *usb_port1));
  EXPECT_TRUE(context()->HasPortPermission(kFooOrigin, *usb_port2));

  EXPECT_TRUE(context()->CanRequestObjectPermission(kBarOrigin));
  EXPECT_FALSE(context()->HasPortPermission(kBarOrigin, *platform_port));
  EXPECT_TRUE(context()->HasPortPermission(kBarOrigin, *usb_port1));
  EXPECT_FALSE(context()->HasPortPermission(kBarOrigin, *usb_port2));

  // TODO(crbug.com/1001242): Add tests for GetGrantedObjects() and
  // GetAllGrantedObjects() once those have been updated to include device
  // permissions granted by policy.
}

TEST_F(SerialChooserContextTest, PolicyAllowOverridesGuard) {
  const auto kFooOrigin = url::Origin::Create(GURL("https://foo.origin"));
  const auto kBarOrigin = url::Origin::Create(GURL("https://bar.origin"));

  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultSerialGuardSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  prefs->SetManagedPref(prefs::kManagedSerialAllowAllPortsForUrls,
                        ReadJson(R"([ "https://foo.origin" ])"));

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();

  EXPECT_FALSE(context()->CanRequestObjectPermission(kFooOrigin));
  EXPECT_TRUE(context()->HasPortPermission(kFooOrigin, *port));
  EXPECT_FALSE(context()->CanRequestObjectPermission(kBarOrigin));
  EXPECT_FALSE(context()->HasPortPermission(kBarOrigin, *port));
}

TEST_F(SerialChooserContextTest, PolicyAllowOverridesBlocked) {
  const auto kFooOrigin = url::Origin::Create(GURL("https://foo.origin"));
  const auto kBarOrigin = url::Origin::Create(GURL("https://bar.origin"));

  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(
      prefs::kManagedSerialBlockedForUrls,
      ReadJson(R"([ "https://foo.origin", "https://bar.origin" ])"));
  prefs->SetManagedPref(prefs::kManagedSerialAllowAllPortsForUrls,
                        ReadJson(R"([ "https://foo.origin" ])"));

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();

  EXPECT_FALSE(context()->CanRequestObjectPermission(kFooOrigin));
  EXPECT_TRUE(context()->HasPortPermission(kFooOrigin, *port));
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
  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = context()->GetGrantedObjects(origin);
  EXPECT_EQ(1u, objects.size());

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      all_origin_objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());
}

TEST_F(SerialChooserContextTest, BlocklistOverridesPolicy) {
  const auto origin = url::Origin::Create(GURL("https://google.com"));

  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedSerialAllowUsbDevicesForUrls,
                        ReadJson(R"([
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
  EXPECT_TRUE(context()->HasPortPermission(origin, *port));

  // Adding a USB device to the blocklist overrides permissions granted by
  // policy.
  SetDynamicBlocklist("usb:18D1:58F0");
  EXPECT_FALSE(context()->HasPortPermission(origin, *port));
}
