// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/serial_chooser_context.h"

#include "base/run_loop.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/chooser_context_base_mock_permission_observer.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/test/browser_task_environment.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SerialChooserContextTest : public testing::Test {
 public:
  SerialChooserContextTest() = default;
  ~SerialChooserContextTest() override = default;

  Profile* profile() { return &profile_; }
  MockPermissionObserver& observer() { return mock_observer_; }

  SerialChooserContext* GetContext(Profile* profile) {
    auto* context = SerialChooserContextFactory::GetForProfile(profile);
    context->AddObserver(&mock_observer_);
    return context;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  MockPermissionObserver mock_observer_;
};

}  // namespace

TEST_F(SerialChooserContextTest, GrantAndRevokeEphemeralPermission) {
  const auto origin = url::Origin::Create(GURL("https://google.com"));

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();

  SerialChooserContext* context = GetContext(profile());
  EXPECT_FALSE(context->HasPortPermission(origin, origin, *port));

  EXPECT_CALL(observer(), OnChooserObjectPermissionChanged(
                              ContentSettingsType::SERIAL_GUARD,
                              ContentSettingsType::SERIAL_CHOOSER_DATA));

  context->GrantPortPermission(origin, origin, *port);
  EXPECT_TRUE(context->HasPortPermission(origin, origin, *port));

  std::vector<std::unique_ptr<ChooserContextBase::Object>> origin_objects =
      context->GetGrantedObjects(origin, origin);
  ASSERT_EQ(1u, origin_objects.size());

  std::vector<std::unique_ptr<ChooserContextBase::Object>> objects =
      context->GetAllGrantedObjects();
  ASSERT_EQ(1u, objects.size());
  EXPECT_EQ(origin.GetURL(), objects[0]->requesting_origin);
  EXPECT_EQ(origin.GetURL(), objects[0]->embedding_origin);
  EXPECT_EQ(origin_objects[0]->value, objects[0]->value);
  EXPECT_EQ(content_settings::SettingSource::SETTING_SOURCE_USER,
            objects[0]->source);
  EXPECT_FALSE(objects[0]->incognito);

  EXPECT_CALL(observer(), OnChooserObjectPermissionChanged(
                              ContentSettingsType::SERIAL_GUARD,
                              ContentSettingsType::SERIAL_CHOOSER_DATA));
  EXPECT_CALL(observer(), OnPermissionRevoked(origin, origin));

  context->RevokeObjectPermission(origin, origin, objects[0]->value);
  EXPECT_FALSE(context->HasPortPermission(origin, origin, *port));
  origin_objects = context->GetGrantedObjects(origin, origin);
  EXPECT_EQ(0u, origin_objects.size());
  objects = context->GetAllGrantedObjects();
  EXPECT_EQ(0u, objects.size());
}

TEST_F(SerialChooserContextTest, GuardPermission) {
  const auto origin = url::Origin::Create(GURL("https://google.com"));

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();

  SerialChooserContext* context = GetContext(profile());
  context->GrantPortPermission(origin, origin, *port);
  EXPECT_TRUE(context->HasPortPermission(origin, origin, *port));

  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(origin.GetURL(), origin.GetURL(),
                                     ContentSettingsType::SERIAL_GUARD,
                                     std::string(), CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(context->HasPortPermission(origin, origin, *port));

  std::vector<std::unique_ptr<ChooserContextBase::Object>> objects =
      context->GetGrantedObjects(origin, origin);
  EXPECT_EQ(0u, objects.size());

  std::vector<std::unique_ptr<ChooserContextBase::Object>> all_origin_objects =
      context->GetAllGrantedObjects();
  EXPECT_EQ(0u, all_origin_objects.size());
}
