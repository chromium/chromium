// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_chooser_context.h"

#include "base/run_loop.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/test/chooser_context_base_mock_permission_observer.h"
#include "content/public/test/browser_task_environment.h"
#include "services/device/public/cpp/hid/fake_hid_manager.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class HidChooserContextTest : public testing::Test {
 public:
  HidChooserContextTest() = default;
  ~HidChooserContextTest() override = default;

  Profile* profile() { return &profile_; }
  permissions::MockPermissionObserver& observer() { return mock_observer_; }
  device::FakeHidManager* hid_manager() { return &hid_manager_; }

  HidChooserContext* GetContext(Profile* profile) {
    auto* context = HidChooserContextFactory::GetForProfile(profile);
    context->AddObserver(&mock_observer_);
    return context;
  }

  void SetUp() override {
    mojo::PendingRemote<device::mojom::HidManager> hid_manager;
    hid_manager_.Bind(hid_manager.InitWithNewPipeAndPassReceiver());
    HidChooserContextFactory::GetForProfile(profile())->SetHidManagerForTesting(
        std::move(hid_manager));
  }

 private:
  device::FakeHidManager hid_manager_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  permissions::MockPermissionObserver mock_observer_;
};

}  // namespace

TEST_F(HidChooserContextTest, GrantAndRevokeEphemeralPermission) {
  const auto origin = url::Origin::Create(GURL("https://google.com"));

  // Leave |serial_number| empty so the device cannot be granted a persistent
  // permission.
  auto device = hid_manager()->CreateAndAddDevice(
      "physical-device-id", 0x1234, 0xabcd, "product-name",
      /*serial_number=*/"", device::mojom::HidBusType::kHIDBusTypeUSB);

  HidChooserContext* context = GetContext(profile());
  EXPECT_FALSE(context->HasDevicePermission(origin, origin, *device));

  EXPECT_CALL(observer(), OnChooserObjectPermissionChanged(
                              ContentSettingsType::HID_GUARD,
                              ContentSettingsType::HID_CHOOSER_DATA));

  context->GrantDevicePermission(origin, origin, *device);
  EXPECT_TRUE(context->HasDevicePermission(origin, origin, *device));

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      origin_objects = context->GetGrantedObjects(origin, origin);
  ASSERT_EQ(1u, origin_objects.size());

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = context->GetAllGrantedObjects();
  ASSERT_EQ(1u, objects.size());
  EXPECT_EQ(origin.GetURL(), objects[0]->requesting_origin);
  EXPECT_EQ(origin.GetURL(), objects[0]->embedding_origin);
  EXPECT_EQ(origin_objects[0]->value, objects[0]->value);
  EXPECT_EQ(content_settings::SettingSource::SETTING_SOURCE_USER,
            objects[0]->source);
  EXPECT_FALSE(objects[0]->incognito);

  EXPECT_CALL(observer(), OnChooserObjectPermissionChanged(
                              ContentSettingsType::HID_GUARD,
                              ContentSettingsType::HID_CHOOSER_DATA));
  EXPECT_CALL(observer(), OnPermissionRevoked(origin, origin));

  context->RevokeObjectPermission(origin, origin, objects[0]->value);
  EXPECT_FALSE(context->HasDevicePermission(origin, origin, *device));
  origin_objects = context->GetGrantedObjects(origin, origin);
  EXPECT_EQ(0u, origin_objects.size());
  objects = context->GetAllGrantedObjects();
  EXPECT_EQ(0u, objects.size());
}

TEST_F(HidChooserContextTest, GrantAndRevokeUsbPersistentPermission) {
  const auto origin = url::Origin::Create(GURL("https://google.com"));

  // USB devices are eligible for persistent permissions if the device reports
  // a serial number string.
  auto device = hid_manager()->CreateAndAddDevice(
      "physical-device-id", 0x1234, 0xabcd, "product-name", "serial-number",
      device::mojom::HidBusType::kHIDBusTypeUSB);

  HidChooserContext* context = GetContext(profile());
  EXPECT_FALSE(context->HasDevicePermission(origin, origin, *device));

  EXPECT_CALL(observer(), OnChooserObjectPermissionChanged(
                              ContentSettingsType::HID_GUARD,
                              ContentSettingsType::HID_CHOOSER_DATA));

  context->GrantDevicePermission(origin, origin, *device);
  EXPECT_TRUE(context->HasDevicePermission(origin, origin, *device));

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      origin_objects = context->GetGrantedObjects(origin, origin);
  ASSERT_EQ(1u, origin_objects.size());

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = context->GetAllGrantedObjects();
  ASSERT_EQ(1u, objects.size());
  EXPECT_EQ(origin.GetURL(), objects[0]->requesting_origin);
  EXPECT_EQ(origin.GetURL(), objects[0]->embedding_origin);
  EXPECT_EQ(origin_objects[0]->value, objects[0]->value);
  EXPECT_EQ(content_settings::SettingSource::SETTING_SOURCE_USER,
            objects[0]->source);
  EXPECT_FALSE(objects[0]->incognito);

  EXPECT_CALL(observer(), OnChooserObjectPermissionChanged(
                              ContentSettingsType::HID_GUARD,
                              ContentSettingsType::HID_CHOOSER_DATA));
  EXPECT_CALL(observer(), OnPermissionRevoked(origin, origin));

  context->RevokeObjectPermission(origin, origin, objects[0]->value);
  EXPECT_FALSE(context->HasDevicePermission(origin, origin, *device));
  origin_objects = context->GetGrantedObjects(origin, origin);
  EXPECT_EQ(0u, origin_objects.size());
  objects = context->GetAllGrantedObjects();
  EXPECT_EQ(0u, objects.size());
}

TEST_F(HidChooserContextTest, GuardPermission) {
  const auto origin = url::Origin::Create(GURL("https://google.com"));

  auto device = device::mojom::HidDeviceInfo::New();
  device->guid = "test-guid";

  HidChooserContext* context = GetContext(profile());
  context->GrantDevicePermission(origin, origin, *device);
  EXPECT_TRUE(context->HasDevicePermission(origin, origin, *device));

  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(origin.GetURL(), origin.GetURL(),
                                     ContentSettingsType::HID_GUARD,
                                     std::string(), CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(context->HasDevicePermission(origin, origin, *device));

  auto objects = context->GetGrantedObjects(origin, origin);
  EXPECT_EQ(0u, objects.size());

  auto all_origin_objects = context->GetAllGrantedObjects();
  EXPECT_EQ(0u, all_origin_objects.size());
}
