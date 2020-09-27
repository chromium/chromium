// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/serial_chooser_context.h"

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/serial/serial_chooser_histograms.h"
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

  device::FakeSerialPortManager& port_manager() { return port_manager_; }
  TestingProfile* profile() { return &profile_; }
  SerialChooserContext* context() { return context_; }
  permissions::MockPermissionObserver& permission_observer() {
    return permission_observer_;
  }
  MockPortObserver& port_observer() { return port_observer_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
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

  EXPECT_FALSE(context()->HasPortPermission(origin, origin, *port));

  EXPECT_CALL(permission_observer(),
              OnChooserObjectPermissionChanged(
                  ContentSettingsType::SERIAL_GUARD,
                  ContentSettingsType::SERIAL_CHOOSER_DATA));

  context()->GrantPortPermission(origin, origin, *port);
  EXPECT_TRUE(context()->HasPortPermission(origin, origin, *port));

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      origin_objects = context()->GetGrantedObjects(origin, origin);
  ASSERT_EQ(1u, origin_objects.size());

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = context()->GetAllGrantedObjects();
  ASSERT_EQ(1u, objects.size());
  EXPECT_EQ(origin.GetURL(), objects[0]->requesting_origin);
  EXPECT_EQ(origin.GetURL(), objects[0]->embedding_origin);
  EXPECT_EQ(origin_objects[0]->value, objects[0]->value);
  EXPECT_EQ(content_settings::SettingSource::SETTING_SOURCE_USER,
            objects[0]->source);
  EXPECT_FALSE(objects[0]->incognito);

  EXPECT_CALL(permission_observer(),
              OnChooserObjectPermissionChanged(
                  ContentSettingsType::SERIAL_GUARD,
                  ContentSettingsType::SERIAL_CHOOSER_DATA));
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin, origin));

  context()->RevokeObjectPermission(origin, origin, objects[0]->value);
  EXPECT_FALSE(context()->HasPortPermission(origin, origin, *port));
  origin_objects = context()->GetGrantedObjects(origin, origin);
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

  EXPECT_FALSE(context()->HasPortPermission(origin, origin, *port));

  EXPECT_CALL(permission_observer(),
              OnChooserObjectPermissionChanged(
                  ContentSettingsType::SERIAL_GUARD,
                  ContentSettingsType::SERIAL_CHOOSER_DATA));

  context()->GrantPortPermission(origin, origin, *port);
  EXPECT_TRUE(context()->HasPortPermission(origin, origin, *port));

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      origin_objects = context()->GetGrantedObjects(origin, origin);
  ASSERT_EQ(1u, origin_objects.size());

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = context()->GetAllGrantedObjects();
  ASSERT_EQ(1u, objects.size());
  EXPECT_EQ(origin.GetURL(), objects[0]->requesting_origin);
  EXPECT_EQ(origin.GetURL(), objects[0]->embedding_origin);
  EXPECT_EQ(origin_objects[0]->value, objects[0]->value);
  EXPECT_EQ(content_settings::SettingSource::SETTING_SOURCE_USER,
            objects[0]->source);
  EXPECT_FALSE(objects[0]->incognito);

  EXPECT_CALL(permission_observer(),
              OnChooserObjectPermissionChanged(
                  ContentSettingsType::SERIAL_GUARD,
                  ContentSettingsType::SERIAL_CHOOSER_DATA));
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin, origin));

  context()->RevokeObjectPermission(origin, origin, objects[0]->value);
  EXPECT_FALSE(context()->HasPortPermission(origin, origin, *port));
  origin_objects = context()->GetGrantedObjects(origin, origin);
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

  context()->GrantPortPermission(origin, origin, *port);
  EXPECT_TRUE(context()->HasPortPermission(origin, origin, *port));

  EXPECT_CALL(permission_observer(),
              OnChooserObjectPermissionChanged(
                  ContentSettingsType::SERIAL_GUARD,
                  ContentSettingsType::SERIAL_CHOOSER_DATA));
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin, origin));

  port_manager().RemovePort(port->token);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(port_observer(), OnPortRemoved(testing::_))
        .WillOnce(
            testing::Invoke([&](const device::mojom::SerialPortInfo& info) {
              EXPECT_EQ(port->token, info.token);
              EXPECT_TRUE(context()->HasPortPermission(origin, origin, info));
              run_loop.Quit();
            }));
    run_loop.Run();
  }
  EXPECT_FALSE(context()->HasPortPermission(origin, origin, *port));
  auto origin_objects = context()->GetGrantedObjects(origin, origin);
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

  context()->GrantPortPermission(origin, origin, *port);
  EXPECT_TRUE(context()->HasPortPermission(origin, origin, *port));

  EXPECT_CALL(permission_observer(),
              OnChooserObjectPermissionChanged(
                  ContentSettingsType::SERIAL_GUARD,
                  ContentSettingsType::SERIAL_CHOOSER_DATA));
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin, origin));

  // Without a display name a persistent permission cannot be recorded and so
  // removing the device will revoke permission.
  port_manager().RemovePort(port->token);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(port_observer(), OnPortRemoved(testing::_))
        .WillOnce(
            testing::Invoke([&](const device::mojom::SerialPortInfo& info) {
              EXPECT_EQ(port->token, info.token);
              EXPECT_TRUE(context()->HasPortPermission(origin, origin, info));
              run_loop.Quit();
            }));
    run_loop.Run();
  }
  EXPECT_FALSE(context()->HasPortPermission(origin, origin, *port));
  auto origin_objects = context()->GetGrantedObjects(origin, origin);
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

  context()->GrantPortPermission(origin, origin, *port);
  EXPECT_TRUE(context()->HasPortPermission(origin, origin, *port));

  EXPECT_CALL(permission_observer(),
              OnChooserObjectPermissionChanged(
                  ContentSettingsType::SERIAL_GUARD,
                  ContentSettingsType::SERIAL_CHOOSER_DATA))
      .Times(0);
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin, origin))
      .Times(0);

  port_manager().RemovePort(port->token);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(port_observer(), OnPortRemoved(testing::_))
        .WillOnce(
            testing::Invoke([&](const device::mojom::SerialPortInfo& info) {
              EXPECT_EQ(port->token, info.token);
              EXPECT_TRUE(context()->HasPortPermission(origin, origin, info));
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  EXPECT_TRUE(context()->HasPortPermission(origin, origin, *port));
  auto origin_objects = context()->GetGrantedObjects(origin, origin);
  EXPECT_EQ(1u, origin_objects.size());
  auto objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(1u, objects.size());

  // Simulate reconnection of the port. It gets a new token but the same
  // persistent ID. This SerialPortInfo should still match the old permission.
  port = CreatePersistentPort("Persistent Port", persistent_id);
  port_manager().AddPort(port.Clone());

  EXPECT_TRUE(context()->HasPortPermission(origin, origin, *port));
}

TEST_F(SerialChooserContextTest, GuardPermission) {
  const auto origin = url::Origin::Create(GURL("https://google.com"));

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();

  context()->GrantPortPermission(origin, origin, *port);
  EXPECT_TRUE(context()->HasPortPermission(origin, origin, *port));

  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(origin.GetURL(), origin.GetURL(),
                                     ContentSettingsType::SERIAL_GUARD,
                                     std::string(), CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(context()->HasPortPermission(origin, origin, *port));

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = context()->GetGrantedObjects(origin, origin);
  EXPECT_EQ(0u, objects.size());

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      all_origin_objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(0u, all_origin_objects.size());
}

TEST_F(SerialChooserContextTest, PolicyGuardPermission) {
  const auto origin = url::Origin::Create(GURL("https://google.com"));

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  context()->GrantPortPermission(origin, origin, *port);

  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultSerialGuardSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  EXPECT_FALSE(context()->CanRequestObjectPermission(origin, origin));
  EXPECT_FALSE(context()->HasPortPermission(origin, origin, *port));

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = context()->GetGrantedObjects(origin, origin);
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
  context()->GrantPortPermission(kFooOrigin, kFooOrigin, *port);
  context()->GrantPortPermission(kBarOrigin, kBarOrigin, *port);

  // Set the default to "ask" so that the policy being tested overrides it.
  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultSerialGuardSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  prefs->SetManagedPref(prefs::kManagedSerialAskForUrls,
                        base::JSONReader::ReadDeprecated(R"(
    [ "https://foo.origin" ]
  )"));

  EXPECT_TRUE(context()->CanRequestObjectPermission(kFooOrigin, kFooOrigin));
  EXPECT_TRUE(context()->HasPortPermission(kFooOrigin, kFooOrigin, *port));
  EXPECT_FALSE(context()->CanRequestObjectPermission(kBarOrigin, kBarOrigin));
  EXPECT_FALSE(context()->HasPortPermission(kBarOrigin, kBarOrigin, *port));

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = context()->GetGrantedObjects(kFooOrigin, kFooOrigin);
  EXPECT_EQ(1u, objects.size());
  objects = context()->GetGrantedObjects(kBarOrigin, kBarOrigin);
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
  context()->GrantPortPermission(kFooOrigin, kFooOrigin, *port);
  context()->GrantPortPermission(kBarOrigin, kBarOrigin, *port);

  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedSerialBlockedForUrls,
                        base::JSONReader::ReadDeprecated(R"(
    [ "https://foo.origin" ]
  )"));

  EXPECT_FALSE(context()->CanRequestObjectPermission(kFooOrigin, kFooOrigin));
  EXPECT_FALSE(context()->HasPortPermission(kFooOrigin, kFooOrigin, *port));
  EXPECT_TRUE(context()->CanRequestObjectPermission(kBarOrigin, kBarOrigin));
  EXPECT_TRUE(context()->HasPortPermission(kBarOrigin, kBarOrigin, *port));

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = context()->GetGrantedObjects(kFooOrigin, kFooOrigin);
  EXPECT_EQ(0u, objects.size());
  objects = context()->GetGrantedObjects(kBarOrigin, kBarOrigin);
  EXPECT_EQ(1u, objects.size());

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      all_origin_objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());
}
