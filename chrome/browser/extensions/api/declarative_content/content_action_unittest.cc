// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/content_action.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "ipc/ipc_message_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

namespace extensions {
namespace {

using base::test::ParseJsonDeprecated;
using testing::HasSubstr;

std::unique_ptr<base::DictionaryValue> SimpleManifest() {
  return DictionaryBuilder()
      .Set("name", "extension")
      .Set("manifest_version", 2)
      .Set("version", "1.0")
      .Build();
}

class RequestContentScriptTest : public ExtensionServiceTestBase {
 public:
  RequestContentScriptTest()
      : extension_(ExtensionBuilder().SetManifest(SimpleManifest()).Build()) {}

  // TODO(rdevlin.cronin): This should be SetUp(), but an issues with invoking
  // InitializeEmptyExtensionService() within SetUp() means that we have to
  // call this manually within every test. This can be cleaned up once said
  // issue is fixed.
  virtual void Init() {
    InitializeEmptyExtensionService();
    static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()))->
        SetReady();
    base::RunLoop().RunUntilIdle();
  }

  Profile* profile() { return profile_.get(); }
  const Extension* extension() { return extension_.get(); }

 private:
  scoped_refptr<const Extension> extension_;
};

TEST(DeclarativeContentActionTest, InvalidCreation) {
  TestExtensionEnvironment env;
  std::string error;
  std::unique_ptr<const ContentAction> result;
  TestingProfile profile;

  // Test wrong data type passed.
  error.clear();
  result = ContentAction::Create(&profile, nullptr, *ParseJsonDeprecated("[]"),
                                 &error);
  EXPECT_THAT(error, HasSubstr("missing instanceType"));
  EXPECT_FALSE(result.get());

  // Test missing instanceType element.
  error.clear();
  result = ContentAction::Create(&profile, nullptr, *ParseJsonDeprecated("{}"),
                                 &error);
  EXPECT_THAT(error, HasSubstr("missing instanceType"));
  EXPECT_FALSE(result.get());

  // Test wrong instanceType element.
  error.clear();
  result = ContentAction::Create(
      &profile, nullptr,
      *ParseJsonDeprecated(
          "{\n"
          "  \"instanceType\": \"declarativeContent.UnknownType\",\n"
          "}"),
      &error);
  EXPECT_THAT(error, HasSubstr("invalid instanceType"));
  EXPECT_FALSE(result.get());
}

TEST(DeclarativeContentActionTest, ShowActionWithoutAction) {
  TestExtensionEnvironment env;

  // We install a component extension because all other extensions have a
  // required action.
  DictionaryBuilder manifest;
  manifest.Set("name", "extension")
      .Set("version", "0.1")
      .Set("manifest_version", 2)
      .Set("description", "an extension");
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(manifest.Build())
          .SetLocation(Manifest::COMPONENT)
          .Build();
  env.GetExtensionService()->AddExtension(extension.get());

  TestingProfile profile;
  std::string error;
  std::unique_ptr<const ContentAction> result = ContentAction::Create(
      &profile, extension.get(),
      *ParseJsonDeprecated(
          "{\n"
          "  \"instanceType\": \"declarativeContent.ShowAction\",\n"
          "}"),
      &error);
  EXPECT_THAT(error, testing::HasSubstr("without an action"));
  ASSERT_FALSE(result.get());
}

class ParameterizedDeclarativeContentActionTest
    : public ::testing::TestWithParam<ExtensionBuilder::ActionType> {};

TEST_P(ParameterizedDeclarativeContentActionTest, ShowAction) {
  TestExtensionEnvironment env;
  content::RenderViewHostTestEnabler rvh_enabler;

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .SetAction(GetParam())
          .SetLocation(Manifest::INTERNAL)
          .Build();

  env.GetExtensionService()->AddExtension(extension.get());

  std::string error;
  TestingProfile profile;
  std::unique_ptr<const ContentAction> result = ContentAction::Create(
      nullptr, extension.get(),
      *ParseJsonDeprecated(
          R"({"instanceType": "declarativeContent.ShowAction"})"),
      &error);
  EXPECT_TRUE(error.empty()) << error;
  ASSERT_TRUE(result.get());

  auto* action_manager = ExtensionActionManager::Get(env.profile());
  ExtensionAction* action = action_manager->GetExtensionAction(*extension);
  ASSERT_TRUE(action);
  if (GetParam() == ExtensionBuilder::ActionType::BROWSER_ACTION) {
    EXPECT_EQ(ActionInfo::TYPE_BROWSER, action->action_type());
    // Switch the default so we properly see the action toggling.
    action->SetIsVisible(ExtensionAction::kDefaultTabId, false);
  } else {
    EXPECT_EQ(ActionInfo::TYPE_PAGE, action->action_type());
  }

  std::unique_ptr<content::WebContents> contents = env.MakeTab();
  const int tab_id = ExtensionTabUtil::GetTabId(contents.get());

  // Currently, the action is not visible on the given tab.
  EXPECT_FALSE(action->GetIsVisible(tab_id));
  ContentAction::ApplyInfo apply_info = {extension.get(), env.profile(),
                                         contents.get(), /*priority=*/100};

  // Apply the content action once. The action should be visible on the tab.
  result->Apply(apply_info);
  EXPECT_TRUE(action->GetIsVisible(tab_id));
  // Apply the content action a second time. The extension action should be
  // visible on the tab, with a "count" of 2 (i.e., two different content
  // actions are keeping it visible).
  result->Apply(apply_info);
  EXPECT_TRUE(action->GetIsVisible(tab_id));
  // Revert one of the content actions. Since two content actions caused the
  // extension action to be visible, it should still be visible after reverting
  // only one.
  result->Revert(apply_info);
  EXPECT_TRUE(action->GetIsVisible(tab_id));
  // Revert the final content action. The extension action should now be hidden
  // again.
  result->Revert(apply_info);
  EXPECT_FALSE(action->GetIsVisible(tab_id));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ParameterizedDeclarativeContentActionTest,
    testing::Values(ExtensionBuilder::ActionType::BROWSER_ACTION,
                    ExtensionBuilder::ActionType::PAGE_ACTION));

TEST(DeclarativeContentActionTest, SetIcon) {
  TestExtensionEnvironment env;
  content::RenderViewHostTestEnabler rvh_enabler;

  // Simulate the process of passing ImageData to SetIcon::Create.
  SkBitmap bitmap;
  EXPECT_TRUE(bitmap.tryAllocN32Pixels(19, 19));
  // Fill the bitmap with red pixels.
  bitmap.eraseARGB(255, 255, 0, 0);
  IPC::Message bitmap_pickle;
  IPC::WriteParam(&bitmap_pickle, bitmap);
  std::string binary_data = std::string(
      static_cast<const char*>(bitmap_pickle.data()), bitmap_pickle.size());
  std::string data64;
  base::Base64Encode(binary_data, &data64);

  std::unique_ptr<base::DictionaryValue> dict =
      DictionaryBuilder()
          .Set("instanceType", "declarativeContent.SetIcon")
          .Set("imageData", DictionaryBuilder().Set("19", data64).Build())
          .Build();

  const Extension* extension = env.MakeExtension(*ParseJsonDeprecated(
      "{\"page_action\": { \"default_title\": \"Extension\" } }"));
  base::HistogramTester histogram_tester;
  TestingProfile profile;
  std::string error;
  ContentAction::SetAllowInvisibleIconsForTest(false);
  std::unique_ptr<const ContentAction> result =
      ContentAction::Create(&profile, extension, *dict, &error);
  ContentAction::SetAllowInvisibleIconsForTest(true);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Extensions.DeclarativeSetIconWasVisible"),
      testing::ElementsAre(base::Bucket(1, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Extensions.DeclarativeSetIconWasVisibleRendered"),
              testing::ElementsAre(base::Bucket(1, 1)));

  ExtensionAction* action = ExtensionActionManager::Get(env.profile())
                                ->GetExtensionAction(*extension);
  std::unique_ptr<content::WebContents> contents = env.MakeTab();
  const int tab_id = ExtensionTabUtil::GetTabId(contents.get());
  EXPECT_FALSE(action->GetIsVisible(tab_id));
  ContentAction::ApplyInfo apply_info = {
    extension, env.profile(), contents.get(), 100
  };

  // The declarative icon shouldn't exist unless the content action is applied.
  EXPECT_TRUE(action->GetDeclarativeIcon(tab_id).IsEmpty());
  result->Apply(apply_info);
  EXPECT_FALSE(action->GetDeclarativeIcon(tab_id).IsEmpty());
  result->Revert(apply_info);
  EXPECT_TRUE(action->GetDeclarativeIcon(tab_id).IsEmpty());
}

TEST(DeclarativeContentActionTest, SetInvisibleIcon) {
  TestExtensionEnvironment env;

  // Simulate the process of passing ImageData to SetIcon::Create.
  SkBitmap bitmap;
  EXPECT_TRUE(bitmap.tryAllocN32Pixels(19, 19));
  bitmap.eraseARGB(0, 0, 0, 0);
  uint32_t* pixels = bitmap.getAddr32(0, 0);
  // Set a single pixel, which isn't enough to consider the icon visible.
  pixels[0] = SkColorSetARGB(255, 255, 0, 0);
  IPC::Message bitmap_pickle;
  IPC::WriteParam(&bitmap_pickle, bitmap);
  std::string binary_data = std::string(
      static_cast<const char*>(bitmap_pickle.data()), bitmap_pickle.size());
  std::string data64;
  base::Base64Encode(binary_data, &data64);

  std::unique_ptr<base::DictionaryValue> dict =
      DictionaryBuilder()
          .Set("instanceType", "declarativeContent.SetIcon")
          .Set("imageData", DictionaryBuilder().Set("19", data64).Build())
          .Build();

  // Expect an error and no instance to be created.
  const Extension* extension = env.MakeExtension(*ParseJsonDeprecated(
      R"({"page_action": {"default_title": "Extension"}})"));
  base::HistogramTester histogram_tester;
  TestingProfile profile;
  std::string error;
  ContentAction::SetAllowInvisibleIconsForTest(false);
  std::unique_ptr<const ContentAction> result =
      ContentAction::Create(&profile, extension, *dict, &error);
  ContentAction::SetAllowInvisibleIconsForTest(true);
  EXPECT_EQ("The specified icon is not sufficiently visible", error);
  EXPECT_FALSE(result);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Extensions.DeclarativeSetIconWasVisible"),
      testing::ElementsAre(base::Bucket(0, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Extensions.DeclarativeSetIconWasVisibleRendered"),
              testing::ElementsAre(base::Bucket(0, 1)));
}

TEST_F(RequestContentScriptTest, MissingScripts) {
  Init();
  std::string error;
  std::unique_ptr<const ContentAction> result = ContentAction::Create(
      profile(), extension(),
      *ParseJsonDeprecated(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"allFrames\": true,\n"
          "  \"matchAboutBlank\": true\n"
          "}"),
      &error);
  EXPECT_THAT(error, testing::HasSubstr("Missing parameter is required"));
  ASSERT_FALSE(result.get());
}

TEST_F(RequestContentScriptTest, CSS) {
  Init();
  std::string error;
  std::unique_ptr<const ContentAction> result = ContentAction::Create(
      profile(), extension(),
      *ParseJsonDeprecated(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"css\": [\"style.css\"]\n"
          "}"),
      &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());
}

TEST_F(RequestContentScriptTest, JS) {
  Init();
  std::string error;
  std::unique_ptr<const ContentAction> result = ContentAction::Create(
      profile(), extension(),
      *ParseJsonDeprecated(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"js\": [\"script.js\"]\n"
          "}"),
      &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());
}

TEST_F(RequestContentScriptTest, CSSBadType) {
  Init();
  std::string error;
  std::unique_ptr<const ContentAction> result = ContentAction::Create(
      profile(), extension(),
      *ParseJsonDeprecated(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"css\": \"style.css\"\n"
          "}"),
      &error);
  ASSERT_FALSE(result.get());
}

TEST_F(RequestContentScriptTest, JSBadType) {
  Init();
  std::string error;
  std::unique_ptr<const ContentAction> result = ContentAction::Create(
      profile(), extension(),
      *ParseJsonDeprecated(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"js\": \"script.js\"\n"
          "}"),
      &error);
  ASSERT_FALSE(result.get());
}

TEST_F(RequestContentScriptTest, AllFrames) {
  Init();
  std::string error;
  std::unique_ptr<const ContentAction> result = ContentAction::Create(
      profile(), extension(),
      *ParseJsonDeprecated(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"js\": [\"script.js\"],\n"
          "  \"allFrames\": true\n"
          "}"),
      &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());
}

TEST_F(RequestContentScriptTest, MatchAboutBlank) {
  Init();
  std::string error;
  std::unique_ptr<const ContentAction> result = ContentAction::Create(
      profile(), extension(),
      *ParseJsonDeprecated(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"js\": [\"script.js\"],\n"
          "  \"matchAboutBlank\": true\n"
          "}"),
      &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());
}

TEST_F(RequestContentScriptTest, AllFramesBadType) {
  Init();
  std::string error;
  std::unique_ptr<const ContentAction> result = ContentAction::Create(
      profile(), extension(),
      *ParseJsonDeprecated(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"js\": [\"script.js\"],\n"
          "  \"allFrames\": null\n"
          "}"),
      &error);
  ASSERT_FALSE(result.get());
}

TEST_F(RequestContentScriptTest, MatchAboutBlankBadType) {
  Init();
  std::string error;
  std::unique_ptr<const ContentAction> result = ContentAction::Create(
      profile(), extension(),
      *ParseJsonDeprecated(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"js\": [\"script.js\"],\n"
          "  \"matchAboutBlank\": null\n"
          "}"),
      &error);
  ASSERT_FALSE(result.get());
}

}  // namespace
}  // namespace extensions
