// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/serial_chooser_context.h"

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
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
#include "components/permissions/test/object_permission_context_base_mock_permission_observer.h"
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
    absl::optional<std::string> name,
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
    scoped_permission_observation_.Observe(context_);
    scoped_port_observation_.Observe(context_);

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
  base::ScopedObservation<
      permissions::ObjectPermissionContextBase,
      permissions::ObjectPermissionContextBase::PermissionObserver>
      scoped_permission_observation_{&permission_observer_};
  MockPortObserver port_observer_;
  base::ScopedObservation<SerialChooserContext,
                          SerialChooserContext::PortObserver,
                          &SerialChooserContext::AddPortObserver,
                          &SerialChooserContext::RemovePortObserver>
      scoped_port_observation_{&port_observer_};
};

}  // namespace

TEST_F(SerialChooserContextTest, GrantAndRevokeEphemeralPermission) {
  base::HistogramTester histogram_tester;

  const auto origin = url::Origin::Create(GURL("https://google.com"));

  auto port_1 = device::mojom::SerialPortInfo::New();
  port_1->token = base::UnguessableToken::Create();

  auto port_2 = CreatePersistentPort("Persistent Port", "ABC123");

  EXPECT_FALSE(context()->HasPortPermission(origin, *port_1));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_2));

  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  absl::make_optional(ContentSettingsType::SERIAL_GUARD),
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
  EXPECT_EQ(content_settings::SettingSource::SETTING_SOURCE_USER,
            objects[0]->source);
  EXPECT_FALSE(objects[0]->incognito);

  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  absl::make_optional(ContentSettingsType::SERIAL_GUARD),
                  ContentSettingsType::SERIAL_CHOOSER_DATA));
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin));

  context()->RevokeObjectPermission(origin, objects[0]->value);
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_1));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_2));
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

  device::mojom::SerialPortInfoPtr port_1 =
      CreatePersistentPort("Persistent Port", "ABC123");

  auto port_2 = device::mojom::SerialPortInfo::New();
  port_2->token = base::UnguessableToken::Create();

  EXPECT_FALSE(context()->HasPortPermission(origin, *port_1));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_2));

  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  absl::make_optional(ContentSettingsType::SERIAL_GUARD),
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
  EXPECT_EQ(content_settings::SettingSource::SETTING_SOURCE_USER,
            objects[0]->source);
  EXPECT_FALSE(objects[0]->incognito);

  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  absl::make_optional(ContentSettingsType::SERIAL_GUARD),
                  ContentSettingsType::SERIAL_CHOOSER_DATA));
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin));

  context()->RevokeObjectPermission(origin, objects[0]->value);
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_1));
  EXPECT_FALSE(context()->HasPortPermission(origin, *port_2));
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
              OnObjectPermissionChanged(
                  absl::make_optional(ContentSettingsType::SERIAL_GUARD),
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
      CreatePersistentPort(/*name=*/absl::nullopt, "ABC123");
  port_manager().AddPort(port.Clone());

  context()->GrantPortPermission(origin, *port);
  EXPECT_TRUE(context()->HasPortPermission(origin, *port));

  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  absl::make_optional(ContentSettingsType::SERIAL_GUARD),
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
              OnObjectPermissionChanged(
                  absl::make_optional(ContentSettingsType::SERIAL_GUARD),
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

  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultSerialGuardSetting,
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
  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultSerialGuardSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  prefs->SetManagedPref(prefs::kManagedSerialAskForUrls,
                        ReadJson(R"([ "https://foo.origin" ])"));

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

  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedSerialBlockedForUrls,
                        ReadJson(R"([ "https://foo.origin" ])"));

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

TEST_F(SerialChooserContextTest, PolicyAllowForUrls) {
  const auto kFooOrigin = url::Origin::Create(GURL("https://foo.origin"));
  const auto kBarOrigin = url::Origin::Create(GURL("https://bar.origin"));

  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedSerialAllowAllPortsForUrls,
                        ReadJson(R"([ "https://foo.origin" ])"));
  prefs->SetManagedPref(prefs::kManagedSerialAllowUsbDevicesForUrls,
                        ReadJson(R"([
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
  EXPECT_TRUE(context()->HasPortPermission(kFooOrigin, *platform_port));
  EXPECT_TRUE(context()->HasPortPermission(kFooOrigin, *usb_port1));
  EXPECT_TRUE(context()->HasPortPermission(kFooOrigin, *usb_port2));

  EXPECT_TRUE(context()->CanRequestObjectPermission(kBarOrigin));
  EXPECT_FALSE(context()->HasPortPermission(kBarOrigin, *platform_port));
  EXPECT_TRUE(context()->HasPortPermission(kBarOrigin, *usb_port1));
  EXPECT_FALSE(context()->HasPortPermission(kBarOrigin, *usb_port2));

  auto foo_objects = context()->GetGrantedObjects(kFooOrigin);
  EXPECT_EQ(1u, foo_objects.size());
  const auto& foo_object = foo_objects.front();
  EXPECT_EQ(kFooOrigin.GetURL(), foo_object->origin);
  EXPECT_EQ(u"Any serial port",
            context()->GetObjectDisplayName(foo_object->value));
  EXPECT_EQ(content_settings::SETTING_SOURCE_POLICY, foo_object->source);
  EXPECT_FALSE(foo_object->incognito);

  auto bar_objects = context()->GetGrantedObjects(kBarOrigin);
  EXPECT_EQ(1u, bar_objects.size());
  const auto& bar_object = bar_objects.front();
  EXPECT_EQ(kBarOrigin.GetURL(), bar_object->origin);
  EXPECT_EQ(u"Nexus One", context()->GetObjectDisplayName(bar_object->value));
  EXPECT_EQ(content_settings::SETTING_SOURCE_POLICY, bar_object->source);
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
    EXPECT_EQ(content_settings::SETTING_SOURCE_POLICY, object->source);
    EXPECT_FALSE(object->incognito);
  }
  EXPECT_TRUE(found_foo_object);
  EXPECT_TRUE(found_bar_object);
}

TEST_F(SerialChooserContextTest, PolicyAllowForUrlsDescriptionStrings) {
  const auto kFooOrigin = url::Origin::Create(GURL("https://foo.origin"));
  const auto kBarOrigin = url::Origin::Create(GURL("https://bar.origin"));

  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedSerialAllowUsbDevicesForUrls,
                        ReadJson(R"([
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

  auto google_objects = context()->GetGrantedObjects(
      url::Origin::Create(GURL("https://google.com")));
  EXPECT_EQ(1u, google_objects.size());
  EXPECT_EQ(u"USB devices from Google Inc.",
            context()->GetObjectDisplayName(google_objects[0]->value));

  auto unknown_vendor_objects = context()->GetGrantedObjects(
      url::Origin::Create(GURL("https://unknown-vendor.com")));
  EXPECT_EQ(1u, unknown_vendor_objects.size());
  EXPECT_EQ(u"USB devices from vendor 18D2",
            context()->GetObjectDisplayName(unknown_vendor_objects[0]->value));

  auto unknown_product_objects = context()->GetGrantedObjects(
      url::Origin::Create(GURL("https://unknown-product.google.com")));
  EXPECT_EQ(1u, unknown_product_objects.size());
  EXPECT_EQ(u"USB device from Google Inc. (product 162E)",
            context()->GetObjectDisplayName(unknown_product_objects[0]->value));

  auto unknown_product_and_vendor_objects = context()->GetGrantedObjects(
      url::Origin::Create(GURL("https://unknown-product.unknown-vendor.com")));
  EXPECT_EQ(1u, unknown_product_and_vendor_objects.size());
  EXPECT_EQ(u"USB device (18D2:162E)",
            context()->GetObjectDisplayName(
                unknown_product_and_vendor_objects[0]->value));
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
  std::vector<std::unique_ptr<SerialChooserContext::Object>> objects =
      context()->GetGrantedObjects(origin);
  EXPECT_EQ(1u, objects.size());

  std::vector<std::unique_ptr<SerialChooserContext::Object>>
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
