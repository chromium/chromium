// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_web_ui.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_web_ui_override_registrar.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_types.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#endif

namespace extensions {

namespace {

std::unique_ptr<KeyedService> BuildOverrideRegistrar(
    content::BrowserContext* context) {
  return std::make_unique<ExtensionWebUIOverrideRegistrar>(context);
}

}  // namespace

class ExtensionWebUITest : public testing::Test {
 public:
  ExtensionWebUITest() = default;

 protected:
  void SetUp() override {
    profile_.reset(new TestingProfile());
    TestExtensionSystem* system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile_.get()));
    extension_service_ = system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
    ExtensionWebUIOverrideRegistrar::GetFactoryInstance()->SetTestingFactory(
        profile_.get(), base::BindRepeating(&BuildOverrideRegistrar));
    ExtensionWebUIOverrideRegistrar::GetFactoryInstance()->Get(profile_.get());
  }

  void TearDown() override {
    profile_.reset();
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<TestingProfile> profile_;
  ExtensionService* extension_service_;
  content::BrowserTaskEnvironment task_environment_;

#if defined OS_CHROMEOS
  chromeos::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif
};

// Test that component extension url overrides have lower priority than
// non-component extension url overrides.
TEST_F(ExtensionWebUITest, ExtensionURLOverride) {
  const char kOverrideResource[] = "1.html";
  // Register a non-component extension.
  DictionaryBuilder manifest;
  manifest.Set(manifest_keys::kName, "ext1")
      .Set(manifest_keys::kVersion, "0.1")
      .Set(manifest_keys::kManifestVersion, 2)
      .Set(std::string(manifest_keys::kChromeURLOverrides),
           DictionaryBuilder().Set("bookmarks", kOverrideResource).Build());
  scoped_refptr<const Extension> ext_unpacked(
      ExtensionBuilder()
          .SetManifest(manifest.Build())
          .SetLocation(Manifest::UNPACKED)
          .SetID("abcdefghijabcdefghijabcdefghijaa")
          .Build());
  extension_service_->AddExtension(ext_unpacked.get());

  const GURL kExpectedUnpackedOverrideUrl =
      ext_unpacked->GetResourceURL(kOverrideResource);
  const GURL kBookmarksUrl(chrome::kChromeUIBookmarksURL);
  GURL changed_url = kBookmarksUrl;
  EXPECT_TRUE(
      ExtensionWebUI::HandleChromeURLOverride(&changed_url, profile_.get()));
  EXPECT_EQ(kExpectedUnpackedOverrideUrl, changed_url);
  EXPECT_TRUE(ExtensionWebUI::HandleChromeURLOverrideReverse(&changed_url,
                                                             profile_.get()));
  EXPECT_EQ(kBookmarksUrl, changed_url);

  GURL url_plus_fragment = kBookmarksUrl.Resolve("#1");
  EXPECT_TRUE(ExtensionWebUI::HandleChromeURLOverride(&url_plus_fragment,
                                                      profile_.get()));
  EXPECT_EQ(kExpectedUnpackedOverrideUrl.Resolve("#1"),
            url_plus_fragment);
  EXPECT_TRUE(ExtensionWebUI::HandleChromeURLOverrideReverse(&url_plus_fragment,
                                                             profile_.get()));
  EXPECT_EQ(kBookmarksUrl.Resolve("#1"), url_plus_fragment);

  // Register a component extension
  const char kOverrideResource2[] = "2.html";
  DictionaryBuilder manifest2;
  manifest2.Set(manifest_keys::kName, "ext2")
      .Set(manifest_keys::kVersion, "0.1")
      .Set(manifest_keys::kManifestVersion, 2)
      .Set(std::string(manifest_keys::kChromeURLOverrides),
           DictionaryBuilder().Set("bookmarks", kOverrideResource2).Build());
  scoped_refptr<const Extension> ext_component(
      ExtensionBuilder()
          .SetManifest(manifest2.Build())
          .SetLocation(Manifest::COMPONENT)
          .SetID("bbabcdefghijabcdefghijabcdefghij")
          .Build());
  extension_service_->AddComponentExtension(ext_component.get());

  // Despite being registered more recently, the component extension should
  // not take precedence over the non-component extension.
  changed_url = kBookmarksUrl;
  EXPECT_TRUE(
      ExtensionWebUI::HandleChromeURLOverride(&changed_url, profile_.get()));
  EXPECT_EQ(kExpectedUnpackedOverrideUrl, changed_url);
  EXPECT_TRUE(ExtensionWebUI::HandleChromeURLOverrideReverse(&changed_url,
                                                             profile_.get()));
  EXPECT_EQ(kBookmarksUrl, changed_url);

  GURL kExpectedComponentOverrideUrl =
      ext_component->GetResourceURL(kOverrideResource2);

  // Unregister non-component extension. Only component extension remaining.
  ExtensionWebUI::UnregisterChromeURLOverrides(
      profile_.get(), URLOverrides::GetChromeURLOverrides(ext_unpacked.get()));
  changed_url = kBookmarksUrl;
  EXPECT_TRUE(
      ExtensionWebUI::HandleChromeURLOverride(&changed_url, profile_.get()));
  EXPECT_EQ(kExpectedComponentOverrideUrl, changed_url);
  EXPECT_TRUE(ExtensionWebUI::HandleChromeURLOverrideReverse(&changed_url,
                                                             profile_.get()));
  EXPECT_EQ(kBookmarksUrl, changed_url);

  // This time the non-component extension was registered more recently and
  // should still take precedence.
  ExtensionWebUI::RegisterOrActivateChromeURLOverrides(
      profile_.get(), URLOverrides::GetChromeURLOverrides(ext_unpacked.get()));
  changed_url = kBookmarksUrl;
  EXPECT_TRUE(
      ExtensionWebUI::HandleChromeURLOverride(&changed_url, profile_.get()));
  EXPECT_EQ(kExpectedUnpackedOverrideUrl, changed_url);
  EXPECT_TRUE(ExtensionWebUI::HandleChromeURLOverrideReverse(&changed_url,
                                                             profile_.get()));
  EXPECT_EQ(kBookmarksUrl, changed_url);
}

TEST_F(ExtensionWebUITest, TestRemovingDuplicateEntriesForHosts) {
  // Test that duplicate entries for a single extension are removed. This could
  // happen because of https://crbug.com/782959.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .SetManifestPath({"chrome_url_overrides", "newtab"}, "newtab.html")
          .Build();

  const GURL newtab_url = extension->GetResourceURL("newtab.html");

  PrefService* prefs = profile_->GetPrefs();
  {
    // Add multiple entries for the same extension.
    DictionaryPrefUpdate update(prefs, ExtensionWebUI::kExtensionURLOverrides);
    base::DictionaryValue* all_overrides = update.Get();
    base::Value newtab_list(base::Value::Type::LIST);
    {
      base::Value newtab(base::Value::Type::DICTIONARY);
      newtab.SetKey("entry", base::Value(newtab_url.spec()));
      newtab.SetKey("active", base::Value(true));
      newtab_list.Append(std::move(newtab));
    }
    {
      base::Value newtab(base::Value::Type::DICTIONARY);
      newtab.SetKey(
          "entry",
          base::Value(extension->GetResourceURL("oldtab.html").spec()));
      newtab.SetKey("active", base::Value(true));
      newtab_list.Append(std::move(newtab));
    }

    all_overrides->SetKey("newtab", std::move(newtab_list));
  }

  extension_service_->AddExtension(extension.get());
  static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile_.get()))
      ->SetReady();
  base::RunLoop().RunUntilIdle();

  // Duplicates should be removed (in response to ExtensionSystem::ready()).
  // Only a single entry should remain.
  const base::DictionaryValue* overrides =
      prefs->GetDictionary(ExtensionWebUI::kExtensionURLOverrides);
  ASSERT_TRUE(overrides);
  const base::Value* newtab_overrides =
      overrides->FindKeyOfType("newtab", base::Value::Type::LIST);
  ASSERT_TRUE(newtab_overrides);
  ASSERT_EQ(1u, newtab_overrides->GetList().size());
  const base::Value& override_dict = newtab_overrides->GetList()[0];
  EXPECT_EQ(newtab_url.spec(), override_dict.FindKey("entry")->GetString());
  EXPECT_TRUE(override_dict.FindKey("active")->GetBool());
}

TEST_F(ExtensionWebUITest, TestFaviconAlwaysAvailable) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").Build();
  extension_service_->AddExtension(extension.get());
  static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile_.get()))
      ->SetReady();

  const GURL kExtensionManifestURL = extension->GetResourceURL("manifest.json");

  std::vector<favicon_base::FaviconRawBitmapResult> favicon_results;
  auto set_favicon_results =
      [](std::vector<favicon_base::FaviconRawBitmapResult>* favicons_out,
         base::RepeatingClosure quit_closure,
         const std::vector<favicon_base::FaviconRawBitmapResult>& favicons) {
        *favicons_out = favicons;
        std::move(quit_closure).Run();
      };

  base::RunLoop run_loop;
  ExtensionWebUI::GetFaviconForURL(
      profile_.get(), kExtensionManifestURL,
      base::BindRepeating(set_favicon_results, &favicon_results,
                          run_loop.QuitClosure()));

  run_loop.Run();
  EXPECT_FALSE(favicon_results.empty());

  // Verify that the favicon bitmaps are not empty and are valid.
  for (const auto& favicon : favicon_results) {
    EXPECT_TRUE(favicon.is_valid());

    SkBitmap bitmap;
    bool result =
        gfx::PNGCodec::Decode(favicon.bitmap_data.get()->front(),
                              favicon.bitmap_data.get()->size(), &bitmap);
    EXPECT_TRUE(result);
    EXPECT_FALSE(bitmap.isNull());
    EXPECT_FALSE(bitmap.drawsNothing());
  }
}

}  // namespace extensions
