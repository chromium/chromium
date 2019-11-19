// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_chooser_context.h"

#include "base/run_loop.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class HidChooserContextTest : public testing::Test {
 public:
  HidChooserContextTest() = default;
  ~HidChooserContextTest() override = default;

  Profile* profile() { return &profile_; }

  HidChooserContext* GetContext(Profile* profile) {
    return HidChooserContextFactory::GetForProfile(profile);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

}  // namespace

TEST_F(HidChooserContextTest, GrantAndRevokeEphemeralPermission) {
  const auto origin = url::Origin::Create(GURL("https://google.com"));

  auto device = device::mojom::HidDeviceInfo::New();
  device->guid = "test-guid";

  HidChooserContext* context = GetContext(profile());
  EXPECT_FALSE(context->HasDevicePermission(origin, origin, *device));
  context->GrantDevicePermission(origin, origin, *device);
  EXPECT_TRUE(context->HasDevicePermission(origin, origin, *device));

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

  context->RevokeObjectPermission(origin, origin, objects[0]->value);
  EXPECT_FALSE(context->HasDevicePermission(origin, origin, *device));
  origin_objects = context->GetGrantedObjects(origin, origin);
  EXPECT_EQ(0u, origin_objects.size());
  objects = context->GetAllGrantedObjects();
  EXPECT_EQ(0u, objects.size());
}
