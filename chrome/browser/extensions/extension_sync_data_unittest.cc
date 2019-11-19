// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_sync_data.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/version.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync/protocol/app_specifics.pb.h"
#include "components/sync/protocol/extension_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace extensions {

namespace {

const char kValidId[] = "abcdefghijklmnopabcdefghijklmnop";
const char kVersion[] = "1.0.0.1";
const char kValidUpdateUrl[] =
    "https://clients2.google.com/service/update2/crx";
const int kValidDisableReasons = disable_reason::DISABLE_USER_ACTION;
const char kName[] = "MyExtension";

const char kBookmarkAppUrl[] = "https://www.example.com/path";
const char kBookmarkAppDescription[] = "My bookmark app";
const char kBookmarkAppScope[] = "https://www.example.com/";
const char kBookmarkIconColor[] = "#F00FED";
const SkColor kBookmarkThemeColor = SK_ColorBLUE;

// Serializes a protobuf structure (entity specifics) into an ExtensionSyncData
// and back again, and confirms that the input is the same as the output.
void ProtobufToSyncDataEqual(const sync_pb::EntitySpecifics& entity) {
  syncer::SyncData sync_data =
      syncer::SyncData::CreateLocalData("sync_tag", "non_unique_title", entity);
  std::unique_ptr<ExtensionSyncData> extension_sync_data =
      ExtensionSyncData::CreateFromSyncData(sync_data);
  ASSERT_TRUE(extension_sync_data.get());
  syncer::SyncData output_sync_data = extension_sync_data->GetSyncData();
  const sync_pb::ExtensionSpecifics& output =
      output_sync_data.GetSpecifics().extension();
  const sync_pb::ExtensionSpecifics& input = entity.extension();

  // Check for field-by-field quality. It'd be nice if we could use
  // AssertionResults here (instead of EXPECT_EQ) so that we could get valid
  // line numbers, but it's not worth the ugliness of the verbose comparison.
  EXPECT_EQ(input.id(), output.id());
  EXPECT_EQ(input.name(), output.name());
  EXPECT_EQ(input.version(), output.version());
  EXPECT_EQ(input.update_url(), output.update_url());
  EXPECT_EQ(input.enabled(), output.enabled());
  EXPECT_EQ(input.incognito_enabled(), output.incognito_enabled());
  EXPECT_EQ(input.remote_install(), output.remote_install());
}

// Serializes an ExtensionSyncData into a protobuf structure and back again, and
// confirms that the input is the same as the output.
void SyncDataToProtobufEqual(const ExtensionSyncData& input) {
  syncer::SyncData sync_data = input.GetSyncData();
  std::unique_ptr<ExtensionSyncData> output =
      ExtensionSyncData::CreateFromSyncData(sync_data);
  ASSERT_TRUE(output.get());

  EXPECT_EQ(input.id(), output->id());
  EXPECT_EQ(input.uninstalled(), output->uninstalled());
  EXPECT_EQ(input.enabled(), output->enabled());
  EXPECT_EQ(input.incognito_enabled(), output->incognito_enabled());
  EXPECT_EQ(input.remote_install(), output->remote_install());
  EXPECT_EQ(input.version(), output->version());
  EXPECT_EQ(input.update_url(), output->update_url());
  EXPECT_EQ(input.name(), output->name());
}

}  // namespace

class ExtensionSyncDataTest : public testing::Test {
};

// Tests the conversion process from a protobuf to an ExtensionSyncData and vice
// versa.
TEST_F(ExtensionSyncDataTest, ExtensionSyncDataForExtension) {
  sync_pb::EntitySpecifics entity;
  sync_pb::ExtensionSpecifics* extension_specifics = entity.mutable_extension();
  extension_specifics->set_id(kValidId);
  extension_specifics->set_update_url(kValidUpdateUrl);
  extension_specifics->set_enabled(false);
  extension_specifics->set_incognito_enabled(true);
  extension_specifics->set_remote_install(false);
  extension_specifics->set_version(kVersion);
  extension_specifics->set_name(kName);

  // Check the serialize-deserialize process for proto to ExtensionSyncData.
  ProtobufToSyncDataEqual(entity);

  // Explicitly test that conversion to an ExtensionSyncData gets the correct
  // result (otherwise we just know that conversion to/from a proto gives us
  // the same result, but don't know that it's right).
  ExtensionSyncData extension_sync_data;
  extension_sync_data.PopulateFromExtensionSpecifics(*extension_specifics);
  EXPECT_EQ(kValidId, extension_sync_data.id());
  EXPECT_EQ(GURL(kValidUpdateUrl), extension_sync_data.update_url());
  EXPECT_FALSE(extension_sync_data.enabled());
  EXPECT_EQ(true, extension_sync_data.incognito_enabled());
  EXPECT_FALSE(extension_sync_data.remote_install());
  EXPECT_EQ(base::Version(kVersion), extension_sync_data.version());
  EXPECT_EQ(std::string(kName), extension_sync_data.name());

  // Check the serialize-deserialize process for ExtensionSyncData to proto.
  SyncDataToProtobufEqual(extension_sync_data);

  // Flip a bit and verify the result is correct.
  extension_specifics->set_incognito_enabled(false);
  ProtobufToSyncDataEqual(entity);

  extension_sync_data.PopulateFromExtensionSpecifics(*extension_specifics);
  EXPECT_FALSE(extension_sync_data.incognito_enabled());

  SyncDataToProtobufEqual(extension_sync_data);
  ProtobufToSyncDataEqual(entity);
}

class AppSyncDataTest : public testing::Test {
 public:
  AppSyncDataTest() {}
  ~AppSyncDataTest() override {}

  void SetRequiredExtensionValues(
      sync_pb::ExtensionSpecifics* extension_specifics) {
    extension_specifics->set_id(kValidId);
    extension_specifics->set_update_url(kValidUpdateUrl);
    extension_specifics->set_version(kVersion);
    extension_specifics->set_enabled(false);
    extension_specifics->set_disable_reasons(kValidDisableReasons);
    extension_specifics->set_incognito_enabled(true);
    extension_specifics->set_remote_install(false);
    extension_specifics->set_name(kName);
  }
};

TEST_F(AppSyncDataTest, SyncDataToExtensionSyncDataForApp) {
  sync_pb::EntitySpecifics entity;
  sync_pb::AppSpecifics* app_specifics = entity.mutable_app();
  app_specifics->set_app_launch_ordinal(
      syncer::StringOrdinal::CreateInitialOrdinal().ToInternalValue());
  app_specifics->set_page_ordinal(
      syncer::StringOrdinal::CreateInitialOrdinal().ToInternalValue());

  SetRequiredExtensionValues(app_specifics->mutable_extension());

  syncer::SyncData sync_data =
      syncer::SyncData::CreateLocalData("sync_tag", "non_unique_title", entity);

  std::unique_ptr<ExtensionSyncData> app_sync_data =
      ExtensionSyncData::CreateFromSyncData(sync_data);
  ASSERT_TRUE(app_sync_data.get());
  EXPECT_EQ(app_specifics->app_launch_ordinal(),
            app_sync_data->app_launch_ordinal().ToInternalValue());
  EXPECT_EQ(app_specifics->page_ordinal(),
            app_sync_data->page_ordinal().ToInternalValue());
}

TEST_F(AppSyncDataTest, ExtensionSyncDataToSyncDataForApp) {
  sync_pb::EntitySpecifics entity;
  sync_pb::AppSpecifics* input_specifics = entity.mutable_app();
  input_specifics->set_app_launch_ordinal(
      syncer::StringOrdinal::CreateInitialOrdinal().ToInternalValue());
  input_specifics->set_page_ordinal(
      syncer::StringOrdinal::CreateInitialOrdinal().ToInternalValue());

  SetRequiredExtensionValues(input_specifics->mutable_extension());

  syncer::SyncData sync_data =
      syncer::SyncData::CreateLocalData("sync_tag", "non_unique_title", entity);
  std::unique_ptr<ExtensionSyncData> app_sync_data =
      ExtensionSyncData::CreateFromSyncData(sync_data);
  ASSERT_TRUE(app_sync_data.get());

  syncer::SyncData output_sync_data = app_sync_data->GetSyncData();
  EXPECT_TRUE(sync_data.GetSpecifics().has_app());
  const sync_pb::AppSpecifics& output_specifics =
      output_sync_data.GetSpecifics().app();
  EXPECT_EQ(input_specifics->SerializeAsString(),
            output_specifics.SerializeAsString());
}

// Ensures that invalid StringOrdinals don't break ExtensionSyncData.
TEST_F(AppSyncDataTest, ExtensionSyncDataInvalidOrdinal) {
  sync_pb::EntitySpecifics entity;
  sync_pb::AppSpecifics* app_specifics = entity.mutable_app();
  // Set the ordinals as invalid.
  app_specifics->set_app_launch_ordinal("");
  app_specifics->set_page_ordinal("");

  SetRequiredExtensionValues(app_specifics->mutable_extension());

  syncer::SyncData sync_data =
      syncer::SyncData::CreateLocalData("sync_tag", "non_unique_title", entity);

  // There should be no issue loading the sync data.
  std::unique_ptr<ExtensionSyncData> app_sync_data =
      ExtensionSyncData::CreateFromSyncData(sync_data);
  ASSERT_TRUE(app_sync_data.get());
  app_sync_data->GetSyncData();
}

TEST_F(AppSyncDataTest, SyncDataToExtensionSyncDataForBookmarkApp) {
  sync_pb::EntitySpecifics entity;
  sync_pb::AppSpecifics* app_specifics = entity.mutable_app();
  app_specifics->set_bookmark_app_url(kBookmarkAppUrl);
  app_specifics->set_bookmark_app_description(kBookmarkAppDescription);
  app_specifics->set_bookmark_app_scope(kBookmarkAppScope);
  app_specifics->set_bookmark_app_icon_color(kBookmarkIconColor);
  app_specifics->set_bookmark_app_theme_color(kBookmarkThemeColor);

  SetRequiredExtensionValues(app_specifics->mutable_extension());

  syncer::SyncData sync_data =
      syncer::SyncData::CreateLocalData("sync_tag", "non_unique_title", entity);

  std::unique_ptr<ExtensionSyncData> app_sync_data =
      ExtensionSyncData::CreateFromSyncData(sync_data);
  ASSERT_TRUE(app_sync_data.get());
  EXPECT_EQ(app_specifics->bookmark_app_url(),
            app_sync_data->bookmark_app_url());
  EXPECT_EQ(app_specifics->bookmark_app_description(),
            app_sync_data->bookmark_app_description());
  EXPECT_EQ(app_specifics->bookmark_app_scope(),
            app_sync_data->bookmark_app_scope());
  EXPECT_EQ(app_specifics->bookmark_app_icon_color(),
            app_sync_data->bookmark_app_icon_color());
  EXPECT_EQ(app_specifics->bookmark_app_theme_color(),
            app_sync_data->bookmark_app_theme_color());

  syncer::SyncData output_sync_data = app_sync_data->GetSyncData();
  EXPECT_TRUE(sync_data.GetSpecifics().has_app());
  const sync_pb::AppSpecifics& output_specifics =
      output_sync_data.GetSpecifics().app();
  EXPECT_EQ(app_specifics->SerializeAsString(),
            output_specifics.SerializeAsString());
}

}  // namespace extensions
