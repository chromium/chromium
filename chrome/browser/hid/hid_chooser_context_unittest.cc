// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_chooser_context.h"

#include <optional>
#include <string_view>

#include "base/barrier_closure.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/hid/mock_hid_device_observer.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/test/object_permission_context_base_mock_permission_observer.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension_features.h"
#include "services/device/public/cpp/hid/hid_blocklist.h"
#include "services/device/public/cpp/test/fake_hid_manager.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

using ::base::test::ParseJson;
using ::base::test::RunClosure;

namespace {

// The device IDs used by the simulated HID device.
constexpr uint16_t kTestVendorId = 0x1234;
constexpr uint16_t kTestProductId = 0xabcd;
constexpr char kTestSerialNumber[] = "serial-number";
constexpr char kTestProductName[] = "product-name";
const char* const kTestPhysicalDeviceIds[] = {"physical-device-id-1",
                                              "physical-device-id-2"};
constexpr char kTestUserEmail[] = "user@example.com";

// The HID usages assigned to the top-level collection of the simulated device.
constexpr uint16_t kTestUsagePage = device::mojom::kPageGenericDesktop;
constexpr uint16_t kTestUsage = device::mojom::kGenericDesktopGamePad;

constexpr char kTestExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

// Main text fixture.
class HidChooserContextTestBase {
 public:
  HidChooserContextTestBase() = default;
  HidChooserContextTestBase(const HidChooserContextTestBase&) = delete;
  HidChooserContextTestBase& operator=(const HidChooserContextTestBase&) =
      delete;
  ~HidChooserContextTestBase() = default;

  void DoSetUp(bool is_affiliated, bool login_user) {
    auto* profile_name = kTestUserEmail;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (login_user) {
      constexpr char kTestUserGaiaId[] = "1111111111";
      auto fake_user_manager = std::make_unique<ash::FakeChromeUserManager>();
      auto* fake_user_manager_ptr = fake_user_manager.get();
      scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
          std::move(fake_user_manager));

      auto account_id =
          AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
      fake_user_manager_ptr->AddUserWithAffiliation(account_id, is_affiliated);
      fake_user_manager_ptr->LoginUser(account_id);
    } else {
      profile_name = ash::kSigninBrowserContextBaseName;
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    profile_ = testing_profile_manager_->CreateTestingProfile(profile_name);
    ASSERT_TRUE(profile_);

    mojo::PendingRemote<device::mojom::HidManager> hid_manager;
    hid_manager_.Bind(hid_manager.InitWithNewPipeAndPassReceiver());
    context_ = HidChooserContextFactory::GetForProfile(profile_);

    // Connect the HidManager and ensure we've received the initial enumeration
    // before continuing.
    base::RunLoop run_loop;
    context_->SetHidManagerForTesting(
        std::move(hid_manager),
        base::BindLambdaForTesting(
            [&run_loop](std::vector<device::mojom::HidDeviceInfoPtr> devices) {
              run_loop.Quit();
            }));
    run_loop.Run();

    scoped_permission_observation_.Observe(context_.get());
    scoped_device_observation_.Observe(context_.get());
  }

  void DoTearDown() {
    // Because HidBlocklist is a singleton it must be cleared after tests run to
    // prevent leakage between tests.
    feature_list_.Reset();
    device::HidBlocklist::Get().ResetToDefaultValuesForTest();
  }

  HidChooserContext* context() { return context_; }
  permissions::MockPermissionObserver& permission_observer() {
    return permission_observer_;
  }
  MockHidDeviceObserver& device_observer() { return device_observer_; }

  device::mojom::HidDeviceInfoPtr CreateDevice(
      std::string_view serial_number,
      const std::string& physical_device_id = kTestPhysicalDeviceIds[0]) {
    auto collection = device::mojom::HidCollectionInfo::New();
    collection->usage =
        device::mojom::HidUsageAndPage::New(kTestUsage, kTestUsagePage);
    collection->collection_type = device::mojom::kHIDCollectionTypeApplication;
    collection->input_reports.push_back(
        device::mojom::HidReportDescription::New());

    auto device = device::mojom::HidDeviceInfo::New();
    device->guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
    device->physical_device_id = physical_device_id;
    device->vendor_id = kTestVendorId;
    device->product_id = kTestProductId;
    device->product_name = kTestProductName;
    device->serial_number = std::string{serial_number};
    device->bus_type = device::mojom::HidBusType::kHIDBusTypeUSB;
    device->collections.push_back(std::move(collection));
    device->protected_input_report_ids =
        device::HidBlocklist::Get().GetProtectedReportIds(
            device::HidBlocklist::kReportTypeInput, kTestVendorId,
            kTestProductId, device->collections);
    device->protected_output_report_ids =
        device::HidBlocklist::Get().GetProtectedReportIds(
            device::HidBlocklist::kReportTypeOutput, kTestVendorId,
            kTestProductId, device->collections);
    device->protected_feature_report_ids =
        device::HidBlocklist::Get().GetProtectedReportIds(
            device::HidBlocklist::kReportTypeFeature, kTestVendorId,
            kTestProductId, device->collections);
    device->is_excluded_by_blocklist =
        device::HidBlocklist::Get().IsVendorProductBlocked(kTestVendorId,
                                                           kTestProductId);
    return device;
  }

  device::mojom::HidDeviceInfoPtr ConnectEphemeralDeviceBlocking() {
    return ConnectDeviceBlocking(CreateDevice(/*serial_number=*/""));
  }

  device::mojom::HidDeviceInfoPtr ConnectPersistentUsbDeviceBlocking() {
    return ConnectDeviceBlocking(CreateDevice(kTestSerialNumber));
  }

  device::mojom::HidDeviceInfoPtr ConnectFidoDeviceBlocking() {
    auto device = CreateDevice(/*serial_number=*/"");
    device->collections[0]->usage->usage_page = device::mojom::kPageFido;
    device->collections[0]->usage->usage = 1;
    return ConnectDeviceBlocking(std::move(device));
  }

  device::mojom::HidDeviceInfoPtr ConnectDeviceBlocking(
      device::mojom::HidDeviceInfoPtr device) {
    base::test::TestFuture<device::mojom::HidDeviceInfoPtr> future_device;
    EXPECT_CALL(device_observer_, OnDeviceAdded).WillOnce([&](const auto& d) {
      future_device.SetValue(d.Clone());
    });
    hid_manager_.AddDevice(std::move(device));
    return future_device.Take();
  }

  device::mojom::HidDeviceInfoPtr DisconnectDeviceBlocking(
      const std::string& device_guid) {
    base::test::TestFuture<device::mojom::HidDeviceInfoPtr> future_device;
    EXPECT_CALL(device_observer_, OnDeviceRemoved).WillOnce([&](const auto& d) {
      future_device.SetValue(d.Clone());
    });
    hid_manager_.RemoveDevice(device_guid);
    return future_device.Take();
  }

  device::mojom::HidDeviceInfoPtr UpdateDeviceBlocking(
      device::mojom::HidDeviceInfoPtr device) {
    base::test::TestFuture<device::mojom::HidDeviceInfoPtr> future_device;
    EXPECT_CALL(device_observer_, OnDeviceChanged).WillOnce([&](const auto& d) {
      future_device.SetValue(d.Clone());
    });
    hid_manager_.ChangeDevice(std::move(device));
    return future_device.Take();
  }

  void SimulateHidManagerConnectionError() {
    hid_manager_.SimulateConnectionError();
  }

  void ExpectObjectPermissionChanged() {
    EXPECT_CALL(permission_observer_,
                OnObjectPermissionChanged(
                    std::make_optional(ContentSettingsType::HID_GUARD),
                    ContentSettingsType::HID_CHOOSER_DATA));
  }

  void GrantDevicePermissionBlocking(
      const url::Origin& origin,
      const device::mojom::HidDeviceInfo& device,
      const std::optional<url::Origin>& embedding_origin = std::nullopt) {
    base::RunLoop loop;
    EXPECT_CALL(permission_observer_,
                OnObjectPermissionChanged(
                    std::make_optional(ContentSettingsType::HID_GUARD),
                    ContentSettingsType::HID_CHOOSER_DATA))
        .WillOnce(RunClosure(loop.QuitClosure()));
    context()->GrantDevicePermission(origin, device, embedding_origin);
    loop.Run();
  }

  void RevokeObjectPermissionBlocking(const url::Origin& origin,
                                      const base::Value::Dict& object) {
    base::RunLoop loop;
    EXPECT_CALL(permission_observer_,
                OnObjectPermissionChanged(
                    std::make_optional(ContentSettingsType::HID_GUARD),
                    ContentSettingsType::HID_CHOOSER_DATA))
        .Times(testing::AtLeast(1))
        .WillOnce(RunClosure(loop.QuitClosure()));
    context()->RevokeObjectPermission(origin, object);
    loop.Run();
  }

  void SetDynamicBlocklist(std::string_view value) {
    feature_list_.Reset();

    std::map<std::string, std::string> parameters;
    parameters[device::kWebHidBlocklistAdditions.name] = std::string{value};
    feature_list_.InitWithFeaturesAndParameters(
        {{device::kWebHidBlocklist, parameters}}, {});

    device::HidBlocklist::Get().ResetToDefaultValuesForTest();
  }

  void SetContentSettingDefaultForOrigin(const url::Origin& origin,
                                         ContentSetting content_setting) {
    HostContentSettingsMapFactory::GetForProfile(profile_)
        ->SetContentSettingDefaultScope(origin.GetURL(), origin.GetURL(),
                                        ContentSettingsType::HID_GUARD,
                                        content_setting);
  }

  void SetContentSettingDefaultPolicy(ContentSetting content_setting) {
    profile_->GetTestingPrefService()->SetManagedPref(
        prefs::kManagedDefaultWebHidGuardSetting,
        std::make_unique<base::Value>(content_setting));
  }

  void SetAskForUrlsPolicy(std::string_view policy) {
    profile_->GetTestingPrefService()->SetManagedPref(
        prefs::kManagedWebHidAskForUrls, ParseJson(policy));
  }

  void SetBlockedForUrlsPolicy(std::string_view policy) {
    profile_->GetTestingPrefService()->SetManagedPref(
        prefs::kManagedWebHidBlockedForUrls, ParseJson(policy));
  }

  void SetAllowDevicesForUrlsPolicy(std::string_view policy) {
    testing_profile_manager_->local_state()->Get()->SetManagedPref(
        prefs::kManagedWebHidAllowDevicesForUrls, ParseJson(policy));
  }

  void SetAllowDevicesForUrlsOnLoginScreenPolicy(std::string_view policy) {
    testing_profile_manager_->local_state()->Get()->SetManagedPref(
        prefs::kManagedWebHidAllowDevicesForUrlsOnLoginScreen,
        ParseJson(policy));
  }

  void SetAllowDevicesWithHidUsagesForUrlsPolicy(std::string_view policy) {
    testing_profile_manager_->local_state()->Get()->SetManagedPref(
        prefs::kManagedWebHidAllowDevicesWithHidUsagesForUrls,
        ParseJson(policy));
  }

  void SetAllowAllDevicesForUrlsPolicy(std::string_view policy) {
    testing_profile_manager_->local_state()->Get()->SetManagedPref(
        prefs::kManagedWebHidAllowAllDevicesForUrls, ParseJson(policy));
  }

 private:
  device::FakeHidManager hid_manager_;
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
#endif

  raw_ptr<HidChooserContext> context_;
  permissions::MockPermissionObserver permission_observer_;
  base::ScopedObservation<
      permissions::ObjectPermissionContextBase,
      permissions::ObjectPermissionContextBase::PermissionObserver>
      scoped_permission_observation_{&permission_observer_};
  MockHidDeviceObserver device_observer_;
  base::ScopedObservation<HidChooserContext, HidChooserContext::DeviceObserver>
      scoped_device_observation_{&device_observer_};
};

class HidChooserContextTest : public HidChooserContextTestBase,
                              public testing::Test {
 public:
  void SetUp() override {
    DoSetUp(/*is_affiliated=*/true, /*login_user=*/true);
  }
  void TearDown() override { DoTearDown(); }
};

}  // namespace

TEST_F(HidChooserContextTest, GrantAndRevokeEphemeralDevice) {
  const auto kOrigin = url::Origin::Create(GURL("https://google.com"));

  // Connect a device that is only eligible for ephemeral permissions.
  auto device = ConnectEphemeralDeviceBlocking();
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());

  // Grant an ephemeral permission.
  GrantDevicePermissionBlocking(kOrigin, *device);
  EXPECT_TRUE(context()->HasDevicePermission(kOrigin, *device));

  std::vector<std::unique_ptr<HidChooserContext::Object>> origin_objects =
      context()->GetGrantedObjects(kOrigin);
  ASSERT_EQ(1u, origin_objects.size());

  std::vector<std::unique_ptr<HidChooserContext::Object>> objects =
      context()->GetAllGrantedObjects();
  ASSERT_EQ(1u, objects.size());
  EXPECT_EQ(kOrigin.GetURL(), objects[0]->origin);
  EXPECT_EQ(origin_objects[0]->value, objects[0]->value);
  EXPECT_EQ(content_settings::SettingSource::kUser, objects[0]->source);
  EXPECT_FALSE(objects[0]->incognito);

  // Revoke the permission.
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(kOrigin));
  RevokeObjectPermissionBlocking(kOrigin, objects[0]->value);
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());
}

TEST_F(HidChooserContextTest, GrantAndForgetEphemeralDevice) {
  const auto kOrigin = url::Origin::Create(GURL("https://google.com"));

  // Connect a device with multiple HID interfaces that is only eligible for
  // ephemeral permissions.
  auto device1 = ConnectEphemeralDeviceBlocking();
  auto device2 = ConnectEphemeralDeviceBlocking();
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device1));
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device2));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());

  // Grant ephemeral permissions.
  GrantDevicePermissionBlocking(kOrigin, *device1);
  GrantDevicePermissionBlocking(kOrigin, *device2);
  EXPECT_TRUE(context()->HasDevicePermission(kOrigin, *device1));
  EXPECT_TRUE(context()->HasDevicePermission(kOrigin, *device2));
  EXPECT_EQ(2u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(2u, context()->GetAllGrantedObjects().size());

  // Forget the ephemeral device.
  base::RunLoop permissions_revoked_loop;
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(kOrigin))
      .WillOnce(RunClosure(permissions_revoked_loop.QuitClosure()));
  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  std::make_optional(ContentSettingsType::HID_GUARD),
                  ContentSettingsType::HID_CHOOSER_DATA));
  context()->RevokeDevicePermission(kOrigin, *device1);
  permissions_revoked_loop.Run();

  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device1));
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device2));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());
}

TEST_F(HidChooserContextTest, GrantTwoEphemeralDevicesForgetOne) {
  const auto kOrigin = url::Origin::Create(GURL("https://google.com"));

  // Connect two devices that are only eligible for ephemeral permissions.
  auto device1 = ConnectDeviceBlocking(CreateDevice(
      /*serial_number=*/"", /*physical_device_id=*/kTestPhysicalDeviceIds[0]));
  auto device2 = ConnectDeviceBlocking(CreateDevice(
      /*serial_number=*/"", /*physical_device_id=*/kTestPhysicalDeviceIds[1]));
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device1));
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device2));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());

  // Grant ephemeral permissions.
  GrantDevicePermissionBlocking(kOrigin, *device1);
  GrantDevicePermissionBlocking(kOrigin, *device2);
  EXPECT_TRUE(context()->HasDevicePermission(kOrigin, *device1));
  EXPECT_TRUE(context()->HasDevicePermission(kOrigin, *device2));
  EXPECT_EQ(2u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(2u, context()->GetAllGrantedObjects().size());

  // Forget the first device.
  base::RunLoop permissions_revoked_loop;
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(kOrigin))
      .WillOnce(RunClosure(permissions_revoked_loop.QuitClosure()));
  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  std::make_optional(ContentSettingsType::HID_GUARD),
                  ContentSettingsType::HID_CHOOSER_DATA));
  context()->RevokeDevicePermission(kOrigin, *device1);
  permissions_revoked_loop.Run();

  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device1));
  EXPECT_TRUE(context()->HasDevicePermission(kOrigin, *device2));
  EXPECT_EQ(1u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(1u, context()->GetAllGrantedObjects().size());
}

TEST_F(HidChooserContextTest, GrantAndDisconnectEphemeralDevice) {
  const auto kOrigin = url::Origin::Create(GURL("https://google.com"));

  // Connect a device that is only eligible for ephemeral permissions.
  auto device = ConnectEphemeralDeviceBlocking();
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());

  // Grant an ephemeral permission.
  GrantDevicePermissionBlocking(kOrigin, *device);
  EXPECT_TRUE(context()->HasDevicePermission(kOrigin, *device));

  auto origin_objects = context()->GetGrantedObjects(kOrigin);
  ASSERT_EQ(1u, origin_objects.size());

  auto objects = context()->GetAllGrantedObjects();
  ASSERT_EQ(1u, objects.size());
  EXPECT_EQ(kOrigin.GetURL(), objects[0]->origin);
  EXPECT_EQ(origin_objects[0]->value, objects[0]->value);
  EXPECT_EQ(content_settings::SettingSource::kUser, objects[0]->source);
  EXPECT_FALSE(objects[0]->incognito);

  // Disconnect the device. Because an ephemeral permission was granted, the
  // permission should be revoked on disconnect.
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(kOrigin));
  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  std::make_optional(ContentSettingsType::HID_GUARD),
                  ContentSettingsType::HID_CHOOSER_DATA));
  DisconnectDeviceBlocking(device->guid);
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());
}

TEST_F(HidChooserContextTest, GrantDisconnectRevokeUsbPersistentDevice) {
  const auto kOrigin = url::Origin::Create(GURL("https://google.com"));

  // Connect a USB device eligible for persistent permissions.
  auto device = ConnectPersistentUsbDeviceBlocking();
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());

  // Grant a persistent permission.
  GrantDevicePermissionBlocking(kOrigin, *device);
  EXPECT_TRUE(context()->HasDevicePermission(kOrigin, *device));

  auto origin_objects = context()->GetGrantedObjects(kOrigin);
  ASSERT_EQ(1u, origin_objects.size());
  auto objects = context()->GetAllGrantedObjects();
  ASSERT_EQ(1u, objects.size());
  EXPECT_EQ(kOrigin.GetURL(), objects[0]->origin);
  EXPECT_EQ(origin_objects[0]->value, objects[0]->value);
  EXPECT_EQ(content_settings::SettingSource::kUser, objects[0]->source);
  EXPECT_FALSE(objects[0]->incognito);

  // Disconnect the device. The permission should not be revoked.
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(kOrigin)).Times(0);
  DisconnectDeviceBlocking(device->guid);
  EXPECT_TRUE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(1u, context()->GetGrantedObjects(kOrigin).size());
  objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(1u, objects.size());

  // Revoke the persistent permission.
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(kOrigin));
  RevokeObjectPermissionBlocking(kOrigin, objects[0]->value);

  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());
}

TEST_F(HidChooserContextTest, GrantForgetUsbPersistentDevice) {
  const auto kOrigin = url::Origin::Create(GURL("https://google.com"));

  // Connect a USB device with multiple HID interfaces eligible for
  // persistent permissions.
  auto device1 = ConnectPersistentUsbDeviceBlocking();
  auto device2 = ConnectPersistentUsbDeviceBlocking();
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device1));
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device2));

  // Grant persistent permissions.
  GrantDevicePermissionBlocking(kOrigin, *device1);
  GrantDevicePermissionBlocking(kOrigin, *device2);
  EXPECT_TRUE(context()->HasDevicePermission(kOrigin, *device1));
  EXPECT_TRUE(context()->HasDevicePermission(kOrigin, *device2));
  EXPECT_EQ(1u, context()->GetGrantedObjects(kOrigin).size());
  auto objects = context()->GetAllGrantedObjects();
  ASSERT_EQ(1u, objects.size());

  // Forget the device by revoking the persistent permission.
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(kOrigin));
  RevokeObjectPermissionBlocking(kOrigin, objects[0]->value);
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device1));
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device2));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());
}

TEST_F(HidChooserContextTest, GuardPermission) {
  const auto kOrigin = url::Origin::Create(GURL("https://google.com"));

  // Connect a device that is only eligible for ephemeral permissions.
  auto device = ConnectEphemeralDeviceBlocking();
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());

  // Grant an ephemeral device permission.
  GrantDevicePermissionBlocking(kOrigin, *device);
  EXPECT_TRUE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(1u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(1u, context()->GetAllGrantedObjects().size());

  // Set the guard permission to CONTENT_SETTING_BLOCK.
  SetContentSettingDefaultForOrigin(kOrigin, CONTENT_SETTING_BLOCK);

  // Check that the device permission is no longer granted.
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());
}

TEST_F(HidChooserContextTest, ConnectionErrorWithEphemeralPermission) {
  const auto kOrigin = url::Origin::Create(GURL("https://google.com"));

  // Connect a device that is only eligible for ephemeral permissions.
  auto device = ConnectEphemeralDeviceBlocking();
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());

  // Grant an ephemeral device permission.
  GrantDevicePermissionBlocking(kOrigin, *device);
  EXPECT_TRUE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(1u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(1u, context()->GetAllGrantedObjects().size());

  // Simulate a connection error. The ephemeral permission should be revoked.
  base::RunLoop loop;
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(kOrigin))
      .WillOnce(RunClosure(loop.QuitClosure()));
  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  std::make_optional(ContentSettingsType::HID_GUARD),
                  ContentSettingsType::HID_CHOOSER_DATA));
  EXPECT_CALL(device_observer(), OnHidManagerConnectionError());
  SimulateHidManagerConnectionError();
  loop.Run();

  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());
}

TEST_F(HidChooserContextTest, ConnectionErrorWithPersistentPermission) {
  const auto kOrigin = url::Origin::Create(GURL("https://google.com"));

  // Connect a USB device eligible for persistent permissions.
  auto device = ConnectPersistentUsbDeviceBlocking();
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());

  // Grant a persistent device permission.
  GrantDevicePermissionBlocking(kOrigin, *device);
  EXPECT_TRUE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(1u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(1u, context()->GetAllGrantedObjects().size());

  // Simulate a connection error. The persistent permission should not be
  // affected.
  base::RunLoop loop;
  EXPECT_CALL(device_observer(), OnHidManagerConnectionError())
      .WillOnce(RunClosure(loop.QuitClosure()));
  EXPECT_CALL(permission_observer(),
              OnObjectPermissionChanged(
                  std::make_optional(ContentSettingsType::HID_GUARD),
                  ContentSettingsType::HID_CHOOSER_DATA));
  SimulateHidManagerConnectionError();
  loop.Run();

  EXPECT_TRUE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(1u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(1u, context()->GetAllGrantedObjects().size());
}

namespace {

device::mojom::HidDeviceInfoPtr CreateDeviceWithOneCollection(
    const std::string& guid) {
  auto device_info = device::mojom::HidDeviceInfo::New();
  device_info->guid = guid;
  auto collection = device::mojom::HidCollectionInfo::New();
  collection->usage = device::mojom::HidUsageAndPage::New(1, 1);
  collection->input_reports.push_back(
      device::mojom::HidReportDescription::New());
  device_info->collections.push_back(std::move(collection));
  return device_info;
}

device::mojom::HidDeviceInfoPtr CreateDeviceWithTwoCollections(
    const std::string& guid) {
  auto device_info = CreateDeviceWithOneCollection(guid);
  auto collection = device::mojom::HidCollectionInfo::New();
  collection->usage = device::mojom::HidUsageAndPage::New(2, 2);
  collection->output_reports.push_back(
      device::mojom::HidReportDescription::New());
  device_info->collections.push_back(std::move(collection));
  return device_info;
}

}  // namespace

TEST_F(HidChooserContextTest, AddChangeRemoveDevice) {
  const char kTestGuid[] = "guid";

  EXPECT_FALSE(context()->GetDeviceInfo(kTestGuid));

  // Connect a partially-initialized device.
  auto partial_device =
      ConnectDeviceBlocking(CreateDeviceWithOneCollection(kTestGuid));
  EXPECT_EQ(partial_device->guid, kTestGuid);
  EXPECT_EQ(partial_device->collections.size(), 1u);
  auto* device_info = context()->GetDeviceInfo(kTestGuid);
  ASSERT_TRUE(device_info);
  EXPECT_EQ(device_info->collections.size(), 1u);

  // Update the device to add another collection.
  auto complete_device =
      UpdateDeviceBlocking(CreateDeviceWithTwoCollections(kTestGuid));
  EXPECT_EQ(complete_device->guid, kTestGuid);
  EXPECT_EQ(complete_device->collections.size(), 2u);
  device_info = context()->GetDeviceInfo(kTestGuid);
  ASSERT_TRUE(device_info);
  EXPECT_EQ(device_info->collections.size(), 2u);

  // Disconnect the device.
  auto removed_device = DisconnectDeviceBlocking(complete_device->guid);
  EXPECT_EQ(removed_device->guid, kTestGuid);
  EXPECT_EQ(removed_device->collections.size(), 2u);
  ASSERT_FALSE(context()->GetDeviceInfo(kTestGuid));
}

namespace {

struct BlocklistTestData {
  const char* blocklist;
  bool expect_device_permission;
} kBlocklistTestData[]{
    {nullptr, true},          {"", true},
    {"1234:abcd::::", false}, {"1234:0001::::", true},
    {"1234:::::", false},     {"2468:::::", true},
    {"::0001:0005::", true},  {"::0001:0006::", true},
    {"::0001:::", true},      {"::ff00:::", true},
};

class HidChooserContextBlocklistTest
    : public HidChooserContextTestBase,
      public testing::TestWithParam<BlocklistTestData> {
 public:
  HidChooserContextBlocklistTest() = default;

  void SetUp() override {
    DoSetUp(/*is_affiliated=*/true, /*login_user=*/true);
  }
  void TearDown() override { DoTearDown(); }
};

}  // namespace

TEST_P(HidChooserContextBlocklistTest, Blocklist) {
  const auto kOrigin = url::Origin::Create(GURL("https://google.com"));

  if (GetParam().blocklist)
    SetDynamicBlocklist(GetParam().blocklist);

  // Connect a device.
  auto device = ConnectPersistentUsbDeviceBlocking();
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());

  // Try to grant permission.
  GrantDevicePermissionBlocking(kOrigin, *device);
  EXPECT_EQ(GetParam().expect_device_permission,
            context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(1u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(1u, context()->GetAllGrantedObjects().size());
}

INSTANTIATE_TEST_SUITE_P(HidChooserContextBlocklistTestInstance,
                         HidChooserContextBlocklistTest,
                         ::testing::ValuesIn(kBlocklistTestData));

TEST_F(HidChooserContextTest, PolicyGuardPermission) {
  const auto kOrigin = url::Origin::Create(GURL("https://google.com"));

  // Connect a device.
  auto device = ConnectPersistentUsbDeviceBlocking();
  EXPECT_TRUE(context()->CanRequestObjectPermission(kOrigin));
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());

  // Grant permission and check that the permission was granted.
  GrantDevicePermissionBlocking(kOrigin, *device);
  EXPECT_TRUE(context()->CanRequestObjectPermission(kOrigin));
  EXPECT_TRUE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(1u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(1u, context()->GetAllGrantedObjects().size());

  // Set a policy to block access to HID devices. After setting the policy, no
  // permissions should be granted and requesting permissions should be blocked.
  SetContentSettingDefaultPolicy(CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(context()->CanRequestObjectPermission(kOrigin));
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());
}

TEST_F(HidChooserContextTest, PolicyAskForUrls) {
  const auto kAskOrigin = url::Origin::Create(GURL("https://ask.origin"));
  const auto kOtherOrigin = url::Origin::Create(GURL("https://other.origin"));

  // Connect a device.
  auto device = ConnectPersistentUsbDeviceBlocking();
  EXPECT_TRUE(context()->CanRequestObjectPermission(kAskOrigin));
  EXPECT_TRUE(context()->CanRequestObjectPermission(kOtherOrigin));
  EXPECT_FALSE(context()->HasDevicePermission(kAskOrigin, *device));
  EXPECT_FALSE(context()->HasDevicePermission(kOtherOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kAskOrigin).size());
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOtherOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());

  // Grant permission for kAskOrigin and kOtherOrigin to access |device|.
  GrantDevicePermissionBlocking(kAskOrigin, *device);
  GrantDevicePermissionBlocking(kOtherOrigin, *device);
  EXPECT_TRUE(context()->CanRequestObjectPermission(kAskOrigin));
  EXPECT_TRUE(context()->CanRequestObjectPermission(kOtherOrigin));
  EXPECT_TRUE(context()->HasDevicePermission(kAskOrigin, *device));
  EXPECT_TRUE(context()->HasDevicePermission(kOtherOrigin, *device));
  EXPECT_EQ(1u, context()->GetGrantedObjects(kAskOrigin).size());
  EXPECT_EQ(1u, context()->GetGrantedObjects(kOtherOrigin).size());
  EXPECT_EQ(2u, context()->GetAllGrantedObjects().size());

  // Set the default guard policy to "block", overriding the granted
  // permissions.
  SetContentSettingDefaultPolicy(CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(context()->CanRequestObjectPermission(kAskOrigin));
  EXPECT_FALSE(context()->CanRequestObjectPermission(kOtherOrigin));
  EXPECT_FALSE(context()->HasDevicePermission(kAskOrigin, *device));
  EXPECT_FALSE(context()->HasDevicePermission(kOtherOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kAskOrigin).size());
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOtherOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());

  // Set the AskForUrls policy to allow kAskOrigin to request permissions.
  // This policy overrides the default guard policy.
  SetAskForUrlsPolicy(R"( [ "https://ask.origin" ] )");
  EXPECT_TRUE(context()->CanRequestObjectPermission(kAskOrigin));
  EXPECT_FALSE(context()->CanRequestObjectPermission(kOtherOrigin));
  EXPECT_TRUE(context()->HasDevicePermission(kAskOrigin, *device));
  EXPECT_FALSE(context()->HasDevicePermission(kOtherOrigin, *device));
  EXPECT_EQ(1u, context()->GetGrantedObjects(kAskOrigin).size());
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOtherOrigin).size());
  EXPECT_EQ(1u, context()->GetAllGrantedObjects().size());
}

TEST_F(HidChooserContextTest, PolicyBlockedForUrls) {
  const auto kBlockedOrigin =
      url::Origin::Create(GURL("https://blocked.origin"));
  const auto kOtherOrigin = url::Origin::Create(GURL("https://other.origin"));

  // Connect a device.
  auto device = ConnectPersistentUsbDeviceBlocking();
  EXPECT_TRUE(context()->CanRequestObjectPermission(kBlockedOrigin));
  EXPECT_TRUE(context()->CanRequestObjectPermission(kOtherOrigin));
  EXPECT_FALSE(context()->HasDevicePermission(kBlockedOrigin, *device));
  EXPECT_FALSE(context()->HasDevicePermission(kOtherOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kBlockedOrigin).size());
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOtherOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());

  // Grant permission for kBlockedOrigin and kOtherOrigin to access |device|.
  GrantDevicePermissionBlocking(kBlockedOrigin, *device);
  GrantDevicePermissionBlocking(kOtherOrigin, *device);
  EXPECT_TRUE(context()->CanRequestObjectPermission(kBlockedOrigin));
  EXPECT_TRUE(context()->CanRequestObjectPermission(kOtherOrigin));
  EXPECT_TRUE(context()->HasDevicePermission(kBlockedOrigin, *device));
  EXPECT_TRUE(context()->HasDevicePermission(kOtherOrigin, *device));
  EXPECT_EQ(1u, context()->GetGrantedObjects(kBlockedOrigin).size());
  EXPECT_EQ(1u, context()->GetGrantedObjects(kOtherOrigin).size());
  EXPECT_EQ(2u, context()->GetAllGrantedObjects().size());

  // Set the BlockedForUrls policy to block kBlockedOrigin from accessing
  // devices or requesting permissions. This policy overrides user-granted
  // permissions and the default guard setting.
  SetBlockedForUrlsPolicy(R"([ "https://blocked.origin" ])");
  EXPECT_FALSE(context()->CanRequestObjectPermission(kBlockedOrigin));
  EXPECT_TRUE(context()->CanRequestObjectPermission(kOtherOrigin));
  EXPECT_FALSE(context()->HasDevicePermission(kBlockedOrigin, *device));
  EXPECT_TRUE(context()->HasDevicePermission(kOtherOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kBlockedOrigin).size());
  EXPECT_EQ(1u, context()->GetGrantedObjects(kOtherOrigin).size());
  EXPECT_EQ(1u, context()->GetAllGrantedObjects().size());
}

namespace {

class HidChooserContextAffiliatedTest : public HidChooserContextTestBase,
                                        public testing::TestWithParam<bool> {
 public:
  HidChooserContextAffiliatedTest() : is_affiliated_(GetParam()) {}

  void SetUp() override { DoSetUp(is_affiliated_, /*login_user=*/true); }
  void TearDown() override { DoTearDown(); }

  bool is_affiliated() const { return is_affiliated_; }

 private:
  bool is_affiliated_;
};

}  // namespace

TEST_P(HidChooserContextAffiliatedTest, PolicyAllowForUrls) {
  const auto kBlockedOrigin =
      url::Origin::Create(GURL("https://blocked.origin"));
  const auto kAskOrigin = url::Origin::Create(GURL("https://ask.origin"));
  const auto kOtherOrigin = url::Origin::Create(GURL("https://other.origin"));

  // Connect a device.
  auto device = ConnectPersistentUsbDeviceBlocking();

  // Set the default content settings to "block" by policy, and set the
  // AskForUrls and BlockedForUrls policies to override the default for
  // kAskOrigin and kBlockedOrigin.
  SetContentSettingDefaultPolicy(CONTENT_SETTING_BLOCK);
  SetAskForUrlsPolicy(R"([ "https://ask.origin" ])");
  SetBlockedForUrlsPolicy(R"([ "https://blocked.origin" ])");
  EXPECT_FALSE(context()->CanRequestObjectPermission(kBlockedOrigin));
  EXPECT_TRUE(context()->CanRequestObjectPermission(kAskOrigin));
  EXPECT_FALSE(context()->CanRequestObjectPermission(kOtherOrigin));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kBlockedOrigin).size());
  EXPECT_EQ(0u, context()->GetGrantedObjects(kAskOrigin).size());
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOtherOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());

  // Set the AllowAllDevicesForUrls policy to grant all three origins permission
  // to access any device.
  SetAllowAllDevicesForUrlsPolicy(R"(
      [
          "https://blocked.origin",
          "https://ask.origin",
          "https://other.origin"
      ])");
  EXPECT_EQ(is_affiliated(),
            context()->HasDevicePermission(kBlockedOrigin, *device));
  EXPECT_EQ(is_affiliated(),
            context()->HasDevicePermission(kAskOrigin, *device));
  EXPECT_EQ(is_affiliated(),
            context()->HasDevicePermission(kOtherOrigin, *device));
  if (is_affiliated()) {
    EXPECT_EQ(1u, context()->GetGrantedObjects(kBlockedOrigin).size());
    EXPECT_EQ(1u, context()->GetGrantedObjects(kAskOrigin).size());
    EXPECT_EQ(1u, context()->GetGrantedObjects(kOtherOrigin).size());
    EXPECT_EQ(3u, context()->GetAllGrantedObjects().size());
  } else {
    EXPECT_EQ(0u, context()->GetGrantedObjects(kBlockedOrigin).size());
    EXPECT_EQ(0u, context()->GetGrantedObjects(kAskOrigin).size());
    EXPECT_EQ(0u, context()->GetGrantedObjects(kOtherOrigin).size());
    EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());
  }

  // Set the AllowAllDevicesForUrls policy back to the default value.
  SetAllowAllDevicesForUrlsPolicy("[]");
  EXPECT_FALSE(context()->HasDevicePermission(kBlockedOrigin, *device));
  EXPECT_FALSE(context()->HasDevicePermission(kAskOrigin, *device));
  EXPECT_FALSE(context()->HasDevicePermission(kOtherOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kBlockedOrigin).size());
  EXPECT_EQ(0u, context()->GetGrantedObjects(kAskOrigin).size());
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOtherOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());

  // Set the AllowDevicesForUrls policy to grant permissions to kAskOrigin and
  // kOtherOrigin by matching kTestVendorId and kTestProductId.
  SetAllowDevicesForUrlsPolicy(R"(
      [
        {
          "devices": [{ "vendor_id": 4660, "product_id": 43981 }],
          "urls": [ "https://ask.origin", "https://other.origin" ]
        }
      ])");
  EXPECT_FALSE(context()->HasDevicePermission(kBlockedOrigin, *device));
  EXPECT_EQ(is_affiliated(),
            context()->HasDevicePermission(kAskOrigin, *device));
  EXPECT_EQ(is_affiliated(),
            context()->HasDevicePermission(kOtherOrigin, *device));
  if (is_affiliated()) {
    EXPECT_EQ(0u, context()->GetGrantedObjects(kBlockedOrigin).size());
    EXPECT_EQ(1u, context()->GetGrantedObjects(kAskOrigin).size());
    EXPECT_EQ(1u, context()->GetGrantedObjects(kOtherOrigin).size());
    EXPECT_EQ(2u, context()->GetAllGrantedObjects().size());
  } else {
    EXPECT_EQ(0u, context()->GetGrantedObjects(kBlockedOrigin).size());
    EXPECT_EQ(0u, context()->GetGrantedObjects(kAskOrigin).size());
    EXPECT_EQ(0u, context()->GetGrantedObjects(kOtherOrigin).size());
    EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());
  }

  // Set the AllowDevicesForUrls policy to give permissions to kBlockedOrigin.
  // This policy overrides the BlockedForUrls policy.
  SetAllowDevicesForUrlsPolicy(R"(
      [
        {
          "devices": [{ "vendor_id": 4660, "product_id": 43981 }],
          "urls": [ "https://blocked.origin" ]
        }
      ])");
  EXPECT_EQ(is_affiliated(),
            context()->HasDevicePermission(kBlockedOrigin, *device));
  EXPECT_FALSE(context()->HasDevicePermission(kAskOrigin, *device));
  EXPECT_FALSE(context()->HasDevicePermission(kOtherOrigin, *device));
  if (is_affiliated()) {
    EXPECT_EQ(1u, context()->GetGrantedObjects(kBlockedOrigin).size());
    EXPECT_EQ(0u, context()->GetGrantedObjects(kAskOrigin).size());
    EXPECT_EQ(0u, context()->GetGrantedObjects(kOtherOrigin).size());
    EXPECT_EQ(1u, context()->GetAllGrantedObjects().size());
  } else {
    EXPECT_EQ(0u, context()->GetGrantedObjects(kBlockedOrigin).size());
    EXPECT_EQ(0u, context()->GetGrantedObjects(kAskOrigin).size());
    EXPECT_EQ(0u, context()->GetGrantedObjects(kOtherOrigin).size());
    EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());
  }

  // Set the AllowDevicesForUrls policy to grant permission to kOtherOrigin to
  // access devices with kTestVendorId and any product ID. A second rule grants
  // permission for kAskOrigin to access a different device with the same vendor
  // ID and a third rule grants permission for kBlockedOrigin to access a
  // different vendor ID. Only the first rule matches the device.
  SetAllowDevicesForUrlsPolicy(R"(
      [
        {
          "devices": [{ "vendor_id": 4660 }],
          "urls": [ "https://other.origin" ]
        },
        {
          "devices": [{ "vendor_id": 4660, "product_id": 1 }],
          "urls": [ "https://ask.origin" ]
        },
        {
          "devices": [{ "vendor_id": 123 }],
          "urls": [ "https://blocked.origin" ]
        }
      ])");
  EXPECT_FALSE(context()->HasDevicePermission(kBlockedOrigin, *device));
  EXPECT_FALSE(context()->HasDevicePermission(kAskOrigin, *device));
  EXPECT_EQ(is_affiliated(),
            context()->HasDevicePermission(kOtherOrigin, *device));
  if (is_affiliated()) {
    EXPECT_EQ(1u, context()->GetGrantedObjects(kBlockedOrigin).size());
    EXPECT_EQ(1u, context()->GetGrantedObjects(kAskOrigin).size());
    EXPECT_EQ(1u, context()->GetGrantedObjects(kOtherOrigin).size());
    EXPECT_EQ(3u, context()->GetAllGrantedObjects().size());
  } else {
    EXPECT_EQ(0u, context()->GetGrantedObjects(kBlockedOrigin).size());
    EXPECT_EQ(0u, context()->GetGrantedObjects(kAskOrigin).size());
    EXPECT_EQ(0u, context()->GetGrantedObjects(kOtherOrigin).size());
    EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());
  }

  // Set the AllowDevicesForUrls policy back to the default value.
  SetAllowDevicesForUrlsPolicy("[]");
  EXPECT_FALSE(context()->HasDevicePermission(kBlockedOrigin, *device));
  EXPECT_FALSE(context()->HasDevicePermission(kAskOrigin, *device));
  EXPECT_FALSE(context()->HasDevicePermission(kOtherOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kBlockedOrigin).size());
  EXPECT_EQ(0u, context()->GetGrantedObjects(kAskOrigin).size());
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOtherOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());

  // Set the AllowDevicesWithHidUsagesForUrls policy to grant permissions to
  // kAskOrigin and kOtherOrigin by matching kTestUsagePage and kTestUsage.
  SetAllowDevicesWithHidUsagesForUrlsPolicy(R"(
      [
        {
          "usages": [{ "usage_page": 1, "usage": 5 }],
          "urls": [ "https://ask.origin", "https://other.origin" ]
        }
      ])");
  EXPECT_FALSE(context()->HasDevicePermission(kBlockedOrigin, *device));
  EXPECT_EQ(is_affiliated(),
            context()->HasDevicePermission(kAskOrigin, *device));
  EXPECT_EQ(is_affiliated(),
            context()->HasDevicePermission(kOtherOrigin, *device));
  if (is_affiliated()) {
    EXPECT_EQ(0u, context()->GetGrantedObjects(kBlockedOrigin).size());
    EXPECT_EQ(1u, context()->GetGrantedObjects(kAskOrigin).size());
    EXPECT_EQ(1u, context()->GetGrantedObjects(kOtherOrigin).size());
    EXPECT_EQ(2u, context()->GetAllGrantedObjects().size());
  } else {
    EXPECT_EQ(0u, context()->GetGrantedObjects(kBlockedOrigin).size());
    EXPECT_EQ(0u, context()->GetGrantedObjects(kAskOrigin).size());
    EXPECT_EQ(0u, context()->GetGrantedObjects(kOtherOrigin).size());
    EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());
  }

  // Set the AllowDevicesWithHidUsagesForUrls policy to give permissions to
  // kBlockedOrigin. This policy overrides the BlockedForUrls policy.
  SetAllowDevicesWithHidUsagesForUrlsPolicy(R"(
      [
        {
          "usages": [{ "usage_page": 1, "usage": 5 }],
          "urls": [ "https://blocked.origin" ]
        }
      ])");
  EXPECT_EQ(is_affiliated(),
            context()->HasDevicePermission(kBlockedOrigin, *device));
  EXPECT_FALSE(context()->HasDevicePermission(kAskOrigin, *device));
  EXPECT_FALSE(context()->HasDevicePermission(kOtherOrigin, *device));
  if (is_affiliated()) {
    EXPECT_EQ(1u, context()->GetGrantedObjects(kBlockedOrigin).size());
    EXPECT_EQ(0u, context()->GetGrantedObjects(kAskOrigin).size());
    EXPECT_EQ(0u, context()->GetGrantedObjects(kOtherOrigin).size());
    EXPECT_EQ(1u, context()->GetAllGrantedObjects().size());
  } else {
    EXPECT_EQ(0u, context()->GetGrantedObjects(kBlockedOrigin).size());
    EXPECT_EQ(0u, context()->GetGrantedObjects(kAskOrigin).size());
    EXPECT_EQ(0u, context()->GetGrantedObjects(kOtherOrigin).size());
    EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());
  }

  // Set the AllowDevicesWithHidUsagesForUrls policy to grant permission to
  // kOtherOrigin to access devices with any usage from kTestUsagePage. A
  // second rule grants permission for kAskOrigin to access devices with a
  // different usage from kTestUsagePage and a third rule grants permission for
  // kBlockedOrigin to access a different usage page. Only the first rule
  // matches the device.
  SetAllowDevicesWithHidUsagesForUrlsPolicy(R"(
      [
        {
          "usages": [{ "usage_page": 1 }],
          "urls": [ "https://other.origin" ]
        },
        {
          "usages": [{ "usage_page": 1, "usage": 1 }],
          "urls": [ "https://ask.origin" ]
        },
        {
          "usages": [{ "usage_page": 123 }],
          "urls": [ "https://blocked.origin" ]
        }
      ])");
  EXPECT_FALSE(context()->HasDevicePermission(kBlockedOrigin, *device));
  EXPECT_FALSE(context()->HasDevicePermission(kAskOrigin, *device));
  EXPECT_EQ(is_affiliated(),
            context()->HasDevicePermission(kOtherOrigin, *device));
  if (is_affiliated()) {
    EXPECT_EQ(1u, context()->GetGrantedObjects(kBlockedOrigin).size());
    EXPECT_EQ(1u, context()->GetGrantedObjects(kAskOrigin).size());
    EXPECT_EQ(1u, context()->GetGrantedObjects(kOtherOrigin).size());
    EXPECT_EQ(3u, context()->GetAllGrantedObjects().size());
  } else {
    EXPECT_EQ(0u, context()->GetGrantedObjects(kBlockedOrigin).size());
    EXPECT_EQ(0u, context()->GetGrantedObjects(kAskOrigin).size());
    EXPECT_EQ(0u, context()->GetGrantedObjects(kOtherOrigin).size());
    EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());
  }

  // Set the AllowDevicesWithHidUsagesForUrls policy back to the default value.
  SetAllowDevicesWithHidUsagesForUrlsPolicy("[]");
  EXPECT_FALSE(context()->HasDevicePermission(kBlockedOrigin, *device));
  EXPECT_FALSE(context()->HasDevicePermission(kAskOrigin, *device));
  EXPECT_FALSE(context()->HasDevicePermission(kOtherOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kBlockedOrigin).size());
  EXPECT_EQ(0u, context()->GetGrantedObjects(kAskOrigin).size());
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOtherOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());
}

TEST_P(HidChooserContextAffiliatedTest, BlocklistOverridesPolicy) {
  const auto kOrigin = url::Origin::Create(GURL("https://test.origin"));

  // Set the blocklist to deny access to devices with kTestVendorId and
  // kTestProductId.
  SetDynamicBlocklist("1234:abcd::::");

  // Connect a device.
  auto device = ConnectPersistentUsbDeviceBlocking();
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());

  // The AllowAllDevicesForUrls policy cannot override the blocklist.
  SetAllowAllDevicesForUrlsPolicy(R"([ "https://test.origin" ])");
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device));
  if (is_affiliated()) {
    EXPECT_EQ(1u, context()->GetGrantedObjects(kOrigin).size());
    EXPECT_EQ(1u, context()->GetAllGrantedObjects().size());
  } else {
    EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
    EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());
  }

  SetAllowAllDevicesForUrlsPolicy("[]");
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());

  // The AllowDevicesForUrls policy cannot override the blocklist.
  SetAllowDevicesForUrlsPolicy(R"(
      [
        {
          "devices": [{ "vendor_id": 4660, "product_id": 43981 }],
          "urls": [ "https://test.origin" ]
        }
      ])");
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device));
  if (is_affiliated()) {
    EXPECT_EQ(1u, context()->GetGrantedObjects(kOrigin).size());
    EXPECT_EQ(1u, context()->GetAllGrantedObjects().size());
  } else {
    EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
    EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());
  }

  SetAllowDevicesForUrlsPolicy("[]");
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());

  // The AllowDevicesWithHidUsagesForUrls policy cannot override the blocklist.
  SetAllowDevicesWithHidUsagesForUrlsPolicy(R"(
      [
        {
          "usages": [{ "usage_page": 1, "usage": 5 }],
          "urls": [ "https://test.origin" ]
        }
      ])");
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device));
  if (is_affiliated()) {
    EXPECT_EQ(1u, context()->GetGrantedObjects(kOrigin).size());
    EXPECT_EQ(1u, context()->GetAllGrantedObjects().size());
  } else {
    EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
    EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());
  }

  SetAllowDevicesWithHidUsagesForUrlsPolicy("[]");
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());
}

TEST_F(HidChooserContextTest, FidoAllowlistOverridesBlocklistDeviceIdRule) {
  const auto kFidoAllowedOrigin = url::Origin::Create(
      GURL("chrome-extension://ckcendljdlmgnhghiaomidhiiclmapok"));
  const auto kOtherOrigin = url::Origin::Create(GURL("https://other.origin"));

  // Configure the blocklist to deny access to devices with kTestVendorId and
  // kTestProductId.
  SetDynamicBlocklist("1234:abcd::::");

  // Connect a FIDO device.
  auto device = ConnectFidoDeviceBlocking();

  // Check that the FIDO device is still blocked. Now it is blocked both for
  // being FIDO and also for matching the device ID rule.
  EXPECT_FALSE(context()->HasDevicePermission(kFidoAllowedOrigin, *device));
  EXPECT_FALSE(context()->HasDevicePermission(kOtherOrigin, *device));

  // Granting permission to the privileged origin succeeds.
  GrantDevicePermissionBlocking(kFidoAllowedOrigin, *device);
  EXPECT_TRUE(context()->HasDevicePermission(kFidoAllowedOrigin, *device));

  // Granting permission to the non-privileged origin fails.
  GrantDevicePermissionBlocking(kOtherOrigin, *device);
  EXPECT_FALSE(context()->HasDevicePermission(kOtherOrigin, *device));
}

TEST_P(HidChooserContextAffiliatedTest, FidoAllowlistAndPolicy) {
  const auto kFidoAndPolicyAllowedOrigin = url::Origin::Create(
      GURL("chrome-extension://ckcendljdlmgnhghiaomidhiiclmapok"));
  const auto kOtherOrigin = url::Origin::Create(GURL("https://other.origin"));

  // Configure the blocklist to deny access to devices with kTestVendorId and
  // kTestProductId.
  SetDynamicBlocklist("1234:abcd::::");

  SetAllowDevicesWithHidUsagesForUrlsPolicy(R"(
      [
        {
          "usages": [{ "usage_page": 61904 }],
          "urls": [ "chrome-extension://ckcendljdlmgnhghiaomidhiiclmapok" ]
        }
      ])");

  // Connect a device matching the first policy rule. If the policy could be set
  // then the policy-granted origin should already have permission.
  auto device = ConnectFidoDeviceBlocking();
  EXPECT_EQ(is_affiliated(), context()->HasDevicePermission(
                                 kFidoAndPolicyAllowedOrigin, *device));
  EXPECT_FALSE(context()->HasDevicePermission(kOtherOrigin, *device));
}

// Boolean parameter means if user is affiliated on the device. Affiliated
// users belong to the domain that owns the device and is only meaningful
// on Chrome OS.
//
// The WebHidAllowDevicesForUrls, WebHidAllowDevicesWithHidUsagesForUrls, and
// WebHidAllowAllDevicesForUrls policies only take effect for affiliated users.
INSTANTIATE_TEST_SUITE_P(
    HidChooserContextAffiliatedTestInstance,
    HidChooserContextAffiliatedTest,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    testing::Values(true, false),
#else
    testing::Values(true),
#endif
    [](const testing::TestParamInfo<HidChooserContextAffiliatedTest::ParamType>&
           info) { return info.param ? "affiliated" : "unaffiliated"; });

namespace {

class HidChooserContextLoginScreenTest : public HidChooserContextTestBase,
                                         public testing::Test {
 public:
  HidChooserContextLoginScreenTest() = default;

  void SetUp() override {
    DoSetUp(/*is_affiliated=*/false, /*login_user=*/false);
  }
  void TearDown() override { DoTearDown(); }
};

}  // namespace

TEST_F(HidChooserContextLoginScreenTest, ApplyPolicyOnLoginScreen) {
  const auto kOrigin = url::Origin::Create(GURL("https://google.com"));

  // Connect a device.
  auto device = ConnectPersistentUsbDeviceBlocking();

  // Set the DeviceLoginScreenWebHidAllowDevicesForUrls policy
  SetAllowDevicesForUrlsOnLoginScreenPolicy(R"(
      [
        {
          "devices": [{ "vendor_id": 4660, "product_id": 43981 }],
          "urls": [ "https://google.com" ]
        }
      ])");

  // The policy has an effect only for IS_CHROMEOS_ASH build, otherwise it is
  // ignored.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(1u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(1u, context()->GetAllGrantedObjects().size());
#else
  EXPECT_FALSE(context()->HasDevicePermission(kOrigin, *device));
  EXPECT_EQ(0u, context()->GetGrantedObjects(kOrigin).size());
  EXPECT_EQ(0u, context()->GetAllGrantedObjects().size());
#endif
}

class HidChooserContextWebViewTest : public HidChooserContextTest {
 public:
  HidChooserContextWebViewTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kEnableWebHidInWebView);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that a permission for WebView can be granted successfully.
// Checks that this permission is *not* accessible outside the embedding app.
TEST_F(HidChooserContextWebViewTest, GrantDevicePermissionToWebView) {
  const auto kEmbeddingOrigin = url::Origin::Create(
      GURL("chrome-extension://" + std::string(kTestExtensionId)));
  const auto kWebViewOrigin = url::Origin::Create(GURL("https://google.com"));

  // Connect an ephemeral device and grant permission to the extension.
  auto device = ConnectEphemeralDeviceBlocking();
  GrantDevicePermissionBlocking(kEmbeddingOrigin, *device);
  EXPECT_TRUE(context()->HasDevicePermission(kEmbeddingOrigin, *device));
  EXPECT_EQ(1u, context()->GetGrantedObjects(kEmbeddingOrigin).size());

  // Grant permission to the embedded WebView.
  GrantDevicePermissionBlocking(kWebViewOrigin, *device, kEmbeddingOrigin);
  EXPECT_TRUE(context()->HasDevicePermission(kEmbeddingOrigin, *device));
  EXPECT_TRUE(context()->HasDevicePermission(kWebViewOrigin, *device,
                                             kEmbeddingOrigin));
  // WebView permission should not leak outside the embedding app context.
  EXPECT_FALSE(context()->HasDevicePermission(kWebViewOrigin, *device));
  const auto kAnotherAppOrigin = url::Origin::Create(
      GURL("chrome-extension://abababababababababababababababababababab"));
  EXPECT_FALSE(context()->HasDevicePermission(kWebViewOrigin, *device,
                                              kAnotherAppOrigin));
}

// Tests that revoking HID device permission from an embedding application
// simultaneously revokes permissions for any associated WebViews that were
// granted access through the embedder.
TEST_F(HidChooserContextWebViewTest, RevokeDevicePermissionFromEmbedder) {
  const auto kEmbeddingOrigin = url::Origin::Create(
      GURL("chrome-extension://" + std::string(kTestExtensionId)));
  const auto kWebViewOrigin = url::Origin::Create(GURL("https://google.com"));

  // Connect an ephemeral device and grant permission to the embedder and
  // WebView.
  auto device = ConnectEphemeralDeviceBlocking();
  GrantDevicePermissionBlocking(kEmbeddingOrigin, *device);
  GrantDevicePermissionBlocking(kWebViewOrigin, *device, kEmbeddingOrigin);
  EXPECT_TRUE(context()->HasDevicePermission(kWebViewOrigin, *device,
                                             kEmbeddingOrigin));

  std::vector<std::unique_ptr<HidChooserContext::Object>> origin_objects =
      context()->GetGrantedObjects(kEmbeddingOrigin);
  ASSERT_EQ(1u, origin_objects.size());

  // Revoke permission from the embedder. WebView's permission should also be
  // revoked.
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(kEmbeddingOrigin));
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(kWebViewOrigin));
  RevokeObjectPermissionBlocking(kEmbeddingOrigin, origin_objects[0]->value);
  EXPECT_FALSE(context()->HasDevicePermission(kEmbeddingOrigin, *device));
  EXPECT_FALSE(context()->HasDevicePermission(kWebViewOrigin, *device,
                                              kEmbeddingOrigin));
}

// Ensures that the revocation of HID device permission from a WebView does not
// interfere with the permissions held by the embedding application.
TEST_F(HidChooserContextWebViewTest, RevokeDevicePermissionFromWebView) {
  const auto kEmbeddingOrigin = url::Origin::Create(
      GURL("chrome-extension://" + std::string(kTestExtensionId)));
  const auto kWebViewOrigin = url::Origin::Create(GURL("https://google.com"));

  // Connect an ephemeral device and grant permission to the embedder and
  // WebView.
  auto device = ConnectEphemeralDeviceBlocking();
  GrantDevicePermissionBlocking(kEmbeddingOrigin, *device);
  GrantDevicePermissionBlocking(kWebViewOrigin, *device, kEmbeddingOrigin);
  EXPECT_TRUE(context()->HasDevicePermission(kWebViewOrigin, *device,
                                             kEmbeddingOrigin));

  // Revoke permission from the WebView. Embedder's permission should not be
  // affected.
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(kWebViewOrigin));
  ExpectObjectPermissionChanged();
  context()->RevokeDevicePermission(kWebViewOrigin, *device, kEmbeddingOrigin);
  EXPECT_TRUE(context()->HasDevicePermission(kEmbeddingOrigin, *device));
  EXPECT_FALSE(context()->HasDevicePermission(kWebViewOrigin, *device,
                                              kEmbeddingOrigin));
}

// Confirms the strict isolation of HID device permissions granted directly to a
// website's origin. Verifies that these permissions are not accessible within
// an embedded WebView, even if belonging to the same website.
TEST_F(HidChooserContextWebViewTest, WebsitePermissionDoesNotLeakToWebView) {
  const auto kEmbeddingOrigin = url::Origin::Create(
      GURL("chrome-extension://" + std::string(kTestExtensionId)));
  const auto kWebViewOrigin = url::Origin::Create(GURL("https://google.com"));

  // Connect an ephemeral device and grant permission to the WebView's origin.
  auto device = ConnectEphemeralDeviceBlocking();
  GrantDevicePermissionBlocking(kWebViewOrigin, *device);
  EXPECT_TRUE(context()->HasDevicePermission(kWebViewOrigin, *device));
  EXPECT_FALSE(context()->HasDevicePermission(kWebViewOrigin, *device,
                                              kEmbeddingOrigin));
}
