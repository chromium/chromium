// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/content_action.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/api/declarative/declarative_constants.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/api/extension_action/action_info_test_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "skia/public/mojom/bitmap.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

namespace extensions {
namespace {

using base::test::ParseJson;
using base::test::ParseJsonDict;
using testing::HasSubstr;
using ContentActionType = declarative_content_constants::ContentActionType;
using extensions::mojom::ManifestLocation;

base::Value::Dict SimpleManifest() {
  return base::Value::Dict()
      .Set("name", "extension")
      .Set("manifest_version", 2)
      .Set("version", "1.0");
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
    auto* extension_system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()));

    extension_system->CreateUserScriptManager();
    service()->AddExtension(extension());
    extension_system->SetReady();
    base::RunLoop().RunUntilIdle();
  }

  const Extension* extension() { return extension_.get(); }

 private:
  scoped_refptr<const Extension> extension_;
};

TEST(DeclarativeContentActionTest, InvalidCreation) {
  TestExtensionEnvironment env;
  std::string error;
  std::unique_ptr<const ContentAction> result;
  TestingProfile profile;
  base::HistogramTester histogram_tester;

  // Test missing instanceType element.
  error.clear();
  result =
      ContentAction::Create(&profile, nullptr, ParseJsonDict("{}"), &error);
  EXPECT_THAT(error, HasSubstr("missing instanceType"));
  EXPECT_FALSE(result.get());
  histogram_tester.ExpectTotalCount(
      "Extensions.DeclarativeContentActionCreated", 0);

  // Test wrong instanceType element.
  error.clear();
  result = ContentAction::Create(&profile, nullptr, ParseJsonDict(R"(
          {
            "instanceType": "declarativeContent.UnknownType",
          })"),
                                 &error);
  EXPECT_THAT(error, HasSubstr("invalid instanceType"));
  EXPECT_FALSE(result.get());
  histogram_tester.ExpectTotalCount(
      "Extensions.DeclarativeContentActionCreated", 0);
}

TEST(DeclarativeContentActionTest, ShowActionWithoutAction) {
  TestExtensionEnvironment env;

  // We install a component extension because all other extensions have a
  // required action.
  auto manifest = base::Value::Dict()
                      .Set("name", "extension")
                      .Set("version", "0.1")
                      .Set("manifest_version", 2)
                      .Set("description", "an extension");
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(std::move(manifest))
          .SetLocation(ManifestLocation::kComponent)
          .Build();
  env.GetExtensionService()->AddExtension(extension.get());

  TestingProfile profile;
  base::HistogramTester histogram_tester;
  std::string error;
  std::unique_ptr<const ContentAction> result =
      ContentAction::Create(&profile, extension.get(), ParseJsonDict(R"(
          {
            "instanceType": "declarativeContent.ShowAction",
          })"),
                            &error);
  EXPECT_THAT(error, testing::HasSubstr("without an action"));
  ASSERT_FALSE(result.get());
  histogram_tester.ExpectTotalCount(
      "Extensions.DeclarativeContentActionCreated", 0);
}

class ParameterizedDeclarativeContentActionTest
    : public ::testing::TestWithParam<ActionInfo::Type> {};

TEST_P(ParameterizedDeclarativeContentActionTest, ShowAction) {
  TestExtensionEnvironment env;
  content::RenderViewHostTestEnabler rvh_enabler;

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .SetAction(GetParam())
          .SetManifestVersion(GetManifestVersionForActionType(GetParam()))
          .SetLocation(ManifestLocation::kInternal)
          .Build();

  env.GetExtensionService()->AddExtension(extension.get());

  std::string error;
  TestingProfile profile;
  base::HistogramTester histogram_tester;
  std::unique_ptr<const ContentAction> result = ContentAction::Create(
      nullptr, extension.get(),
      ParseJsonDict(R"({"instanceType": "declarativeContent.ShowAction"})"),
      &error);
  EXPECT_TRUE(error.empty()) << error;
  ASSERT_TRUE(result.get());

  histogram_tester.ExpectUniqueSample(
      "Extensions.DeclarativeContentActionCreated",
      ContentActionType::kShowAction, 1);
  histogram_tester.ExpectTotalCount(
      "Extensions.DeclarativeContentActionCreated", 1);

  auto* action_manager = ExtensionActionManager::Get(env.profile());
  ExtensionAction* action = action_manager->GetExtensionAction(*extension);
  ASSERT_TRUE(action);
  if (GetParam() == ActionInfo::Type::kBrowser) {
    EXPECT_EQ(ActionInfo::Type::kBrowser, action->action_type());
    // Switch the default so we properly see the action toggling.
    action->SetIsVisible(ExtensionAction::kDefaultTabId, false);
  } else {
    EXPECT_EQ(ActionInfo::Type::kPage, action->action_type());
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

INSTANTIATE_TEST_SUITE_P(All,
                         ParameterizedDeclarativeContentActionTest,
                         testing::Values(ActionInfo::Type::kBrowser,
                                         ActionInfo::Type::kPage));

TEST(DeclarativeContentActionTest, SetIcon) {
  enum Mode { Base64, Mojo, MojoHuge };
  for (Mode mode : {Base64, Mojo, MojoHuge}) {
    SCOPED_TRACE(mode);

    TestExtensionEnvironment env;
    content::RenderViewHostTestEnabler rvh_enabler;

    // Simulate the process of passing ImageData to SetIcon::Create.
    SkBitmap bitmap;
    EXPECT_TRUE(bitmap.tryAllocN32Pixels(19, 19));
    bitmap.eraseARGB(255, 255, 0, 0);

    base::Value::Dict dict;
    dict.Set("instanceType", "declarativeContent.SetIcon");
    switch (mode) {
      case Base64: {
        std::string data64 =
            base::Base64Encode(skia::mojom::InlineBitmap::Serialize(&bitmap));
        dict.Set("imageData", base::Value::Dict().Set("19", data64));
        break;
      }
      case Mojo: {
        std::vector<uint8_t> s = skia::mojom::InlineBitmap::Serialize(&bitmap);
        // Explicit base::Value() for TYPE_BINARY.
        dict.Set("imageData",
                 base::Value::Dict().Set("19", base::Value(std::move(s))));
        break;
      }
      case MojoHuge: {
        // Normal skia::mojom::Bitmaps would serialize as a SharedMemory handle,
        // which is not valid when serializing to a string. We use InlineBitmap
        // instead, and this case verifies it does the right thing for a large
        // image.
        const int dimension =
            std::ceil(std::sqrt(mojo_base::BigBuffer::kMaxInlineBytes));
        EXPECT_TRUE(bitmap.tryAllocN32Pixels(dimension / 4 + 1, dimension));
        EXPECT_GT(bitmap.computeByteSize(),
                  mojo_base::BigBuffer::kMaxInlineBytes);
        bitmap.eraseARGB(255, 255, 0, 0);
        std::vector<uint8_t> s = skia::mojom::InlineBitmap::Serialize(&bitmap);
        // Explicit base::Value() for TYPE_BINARY.
        dict.Set("imageData",
                 base::Value::Dict().Set("19", base::Value(std::move(s))));
        break;
      }
    }

    const Extension* extension = env.MakeExtension(
        ParseJsonDict(R"({"page_action": {"default_title": "Extension"}})"));
    base::HistogramTester histogram_tester;
    TestingProfile profile;
    std::string error;
    ContentAction::SetAllowInvisibleIconsForTest(false);
    std::unique_ptr<const ContentAction> result =
        ContentAction::Create(&profile, extension, dict, &error);
    ContentAction::SetAllowInvisibleIconsForTest(true);
    EXPECT_EQ("", error);
    ASSERT_TRUE(result.get());
    histogram_tester.ExpectUniqueSample(
        "Extensions.DeclarativeContentActionCreated",
        ContentActionType::kSetIcon, 1);
    histogram_tester.ExpectTotalCount(
        "Extensions.DeclarativeContentActionCreated", 1);

    ExtensionAction* action = ExtensionActionManager::Get(env.profile())
                                  ->GetExtensionAction(*extension);
    std::unique_ptr<content::WebContents> contents = env.MakeTab();
    const int tab_id = ExtensionTabUtil::GetTabId(contents.get());
    EXPECT_FALSE(action->GetIsVisible(tab_id));
    ContentAction::ApplyInfo apply_info = {extension, env.profile(),
                                           contents.get(), 100};

    // The declarative icon shouldn't exist unless the content action is
    // applied.
    EXPECT_TRUE(action->GetDeclarativeIcon(tab_id).IsEmpty());
    result->Apply(apply_info);
    EXPECT_FALSE(action->GetDeclarativeIcon(tab_id).IsEmpty());
    result->Revert(apply_info);
    EXPECT_TRUE(action->GetDeclarativeIcon(tab_id).IsEmpty());
  }
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
  std::string data64 =
      base::Base64Encode(skia::mojom::InlineBitmap::Serialize(&bitmap));

  base::Value::Dict dict =
      base::Value::Dict()
          .Set("instanceType", "declarativeContent.SetIcon")
          .Set("imageData", base::Value::Dict().Set("19", data64));

  // Expect an error and no instance to be created.
  const Extension* extension = env.MakeExtension(
      ParseJsonDict(R"({"page_action": {"default_title": "Extension"}})"));
  base::HistogramTester histogram_tester;
  TestingProfile profile;
  std::string error;
  ContentAction::SetAllowInvisibleIconsForTest(false);
  std::unique_ptr<const ContentAction> result =
      ContentAction::Create(&profile, extension, dict, &error);
  ContentAction::SetAllowInvisibleIconsForTest(true);
  EXPECT_EQ("The specified icon is not sufficiently visible", error);
  EXPECT_FALSE(result);
  histogram_tester.ExpectTotalCount(
      "Extensions.DeclarativeContentActionCreated", 0);
}

TEST_F(RequestContentScriptTest, MissingScripts) {
  Init();
  std::string error;
  base::HistogramTester histogram_tester;
  std::unique_ptr<const ContentAction> result =
      ContentAction::Create(profile(), extension(), ParseJsonDict(R"(
          {
            "instanceType": "declarativeContent.RequestContentScript",
            "allFrames": true,
            "matchAboutBlank": true
          })"),
                            &error);
  EXPECT_THAT(error, testing::HasSubstr("Missing parameter is required"));
  ASSERT_FALSE(result.get());
  histogram_tester.ExpectTotalCount(
      "Extensions.DeclarativeContentActionCreated", 0);
}

TEST_F(RequestContentScriptTest, CSS) {
  Init();
  std::string error;
  base::HistogramTester histogram_tester;
  std::unique_ptr<const ContentAction> result =
      ContentAction::Create(profile(), extension(), ParseJsonDict(R"(
          {
            "instanceType": "declarativeContent.RequestContentScript",
            "css": ["style.css"]
          })"),
                            &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());
  histogram_tester.ExpectUniqueSample(
      "Extensions.DeclarativeContentActionCreated",
      ContentActionType::kRequestContentScript, 1);
  histogram_tester.ExpectTotalCount(
      "Extensions.DeclarativeContentActionCreated", 1);
}

TEST_F(RequestContentScriptTest, JS) {
  Init();
  std::string error;
  base::HistogramTester histogram_tester;
  std::unique_ptr<const ContentAction> result =
      ContentAction::Create(profile(), extension(), ParseJsonDict(R"(
          {
            "instanceType": "declarativeContent.RequestContentScript",
            "js": ["script.js"]
          })"),
                            &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());
  histogram_tester.ExpectUniqueSample(
      "Extensions.DeclarativeContentActionCreated",
      ContentActionType::kRequestContentScript, 1);
  histogram_tester.ExpectTotalCount(
      "Extensions.DeclarativeContentActionCreated", 1);
}

TEST_F(RequestContentScriptTest, CSSBadType) {
  Init();
  std::string error;
  base::HistogramTester histogram_tester;
  std::unique_ptr<const ContentAction> result =
      ContentAction::Create(profile(), extension(), ParseJsonDict(R"(
          {
            "instanceType": "declarativeContent.RequestContentScript",
            "css": "style.css"
          })"),
                            &error);
  ASSERT_FALSE(result.get());
  histogram_tester.ExpectTotalCount(
      "Extensions.DeclarativeContentActionCreated", 0);
}

TEST_F(RequestContentScriptTest, JSBadType) {
  Init();
  std::string error;
  base::HistogramTester histogram_tester;
  std::unique_ptr<const ContentAction> result =
      ContentAction::Create(profile(), extension(), ParseJsonDict(R"(
          {
            "instanceType": "declarativeContent.RequestContentScript",
            "js": "script.js"
          })"),
                            &error);
  ASSERT_FALSE(result.get());
  histogram_tester.ExpectTotalCount(
      "Extensions.DeclarativeContentActionCreated", 0);
}

TEST_F(RequestContentScriptTest, AllFrames) {
  Init();
  std::string error;
  base::HistogramTester histogram_tester;
  std::unique_ptr<const ContentAction> result =
      ContentAction::Create(profile(), extension(), ParseJsonDict(R"(
          {
            "instanceType": "declarativeContent.RequestContentScript",
            "js": ["script.js"],
            "allFrames": true
          })"),
                            &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());
  histogram_tester.ExpectUniqueSample(
      "Extensions.DeclarativeContentActionCreated",
      ContentActionType::kRequestContentScript, 1);
  histogram_tester.ExpectTotalCount(
      "Extensions.DeclarativeContentActionCreated", 1);
}

TEST_F(RequestContentScriptTest, MatchAboutBlank) {
  Init();
  std::string error;
  base::HistogramTester histogram_tester;
  std::unique_ptr<const ContentAction> result =
      ContentAction::Create(profile(), extension(), ParseJsonDict(R"(
          {
            "instanceType": "declarativeContent.RequestContentScript",
            "js": ["script.js"],
            "matchAboutBlank": true
          })"),
                            &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());
  histogram_tester.ExpectUniqueSample(
      "Extensions.DeclarativeContentActionCreated",
      ContentActionType::kRequestContentScript, 1);
  histogram_tester.ExpectTotalCount(
      "Extensions.DeclarativeContentActionCreated", 1);
}

TEST_F(RequestContentScriptTest, AllFramesBadType) {
  Init();
  std::string error;
  base::HistogramTester histogram_tester;
  std::unique_ptr<const ContentAction> result =
      ContentAction::Create(profile(), extension(), ParseJsonDict(R"(
          {
            "instanceType": "declarativeContent.RequestContentScript",
            "js": ["script.js"],
            "allFrames": null
          })"),
                            &error);
  ASSERT_FALSE(result.get());
  histogram_tester.ExpectTotalCount(
      "Extensions.DeclarativeContentActionCreated", 0);
}

TEST_F(RequestContentScriptTest, MatchAboutBlankBadType) {
  Init();
  std::string error;
  base::HistogramTester histogram_tester;
  std::unique_ptr<const ContentAction> result =
      ContentAction::Create(profile(), extension(), ParseJsonDict(R"(
          {
            "instanceType": "declarativeContent.RequestContentScript",
            "js": ["script.js"],
            "matchAboutBlank": null
          })"),
                            &error);
  ASSERT_FALSE(result.get());
  histogram_tester.ExpectTotalCount(
      "Extensions.DeclarativeContentActionCreated", 0);
}

}  // namespace
}  // namespace extensions
