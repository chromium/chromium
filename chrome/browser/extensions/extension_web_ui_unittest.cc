// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_web_ui.h"

#include <memory>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/extension_web_ui_override_registrar.h"
#include "chrome/browser/extensions/external_provider_manager.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_types.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/mock_external_provider.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/api/chrome_url_overrides.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/users/user_manager_delegate_impl.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#endif

using extensions::mojom::ManifestLocation;

namespace extensions {

namespace {

constexpr char kNtpOverrideExtensionId[] = "feclidjhghfjpipmbpajpkdeemmjhlei";

std::unique_ptr<KeyedService> BuildOverrideRegistrar(
    content::BrowserContext* context) {
  return std::make_unique<ExtensionWebUIOverrideRegistrar>(context);
}

class TestOverrideRegistrarObserver
    : public ExtensionWebUIOverrideRegistrar::Observer {
 public:
  TestOverrideRegistrarObserver() = default;
  ~TestOverrideRegistrarObserver() override = default;

  void OnExtensionOverrideAdded(const Extension& extension) override {
    loaded_extensions_.push_back(extension.id());
  }

  void OnExtensionOverrideRemoved(const Extension& extension) override {
    unloaded_extensions_.push_back(extension.id());
  }

  const std::vector<ExtensionId>& loaded_extensions() const {
    return loaded_extensions_;
  }
  const std::vector<ExtensionId>& unloaded_extensions() const {
    return unloaded_extensions_;
  }

 private:
  std::vector<ExtensionId> loaded_extensions_;
  std::vector<ExtensionId> unloaded_extensions_;
};

}  // namespace

class ExtensionWebUITest : public testing::Test {
 public:
  ExtensionWebUITest() = default;

 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    TestExtensionSystem* system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile_.get()));
    system->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                                   base::FilePath(), false);
    ExtensionWebUIOverrideRegistrar::GetFactoryInstance()->SetTestingFactory(
        profile_.get(), base::BindRepeating(&BuildOverrideRegistrar));
    ExtensionWebUIOverrideRegistrar::GetFactoryInstance()->Get(profile_.get());
  }

  void TearDown() override {
    profile_.reset();
    base::RunLoop().RunUntilIdle();
  }

  ExtensionRegistrar* registrar() {
    return ExtensionRegistrar::Get(profile_.get());
  }

  std::unique_ptr<TestingProfile> profile_;
  content::BrowserTaskEnvironment task_environment_;

#if BUILDFLAG(IS_CHROMEOS)
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  user_manager::ScopedUserManager user_manager_{
      std::make_unique<user_manager::UserManagerImpl>(
          std::make_unique<ash::UserManagerDelegateImpl>(),
          g_browser_process->local_state(),
          ash::CrosSettings::Get())};
#endif
};

// Test that component extension url overrides have lower priority than
// non-component extension url overrides.
TEST_F(ExtensionWebUITest, ExtensionURLOverride) {
  const char kOverrideResource[] = "1.html";
  // Register a non-component extension.
  auto manifest =
      base::Value::Dict()
          .Set(manifest_keys::kName, "ext1")
          .Set(manifest_keys::kVersion, "0.1")
          .Set(manifest_keys::kManifestVersion, 2)
          .Set(api::chrome_url_overrides::ManifestKeys::kChromeUrlOverrides,
               base::Value::Dict().Set("bookmarks", kOverrideResource));
  scoped_refptr<const Extension> ext_unpacked(
      ExtensionBuilder()
          .SetManifest(std::move(manifest))
          .SetLocation(ManifestLocation::kUnpacked)
          .SetID("abcdefghijabcdefghijabcdefghijaa")
          .Build());
  registrar()->AddExtension(ext_unpacked.get());

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
  auto manifest2 =
      base::Value::Dict()
          .Set(manifest_keys::kName, "ext2")
          .Set(manifest_keys::kVersion, "0.1")
          .Set(manifest_keys::kManifestVersion, 2)
          .Set(api::chrome_url_overrides::ManifestKeys::kChromeUrlOverrides,
               base::Value::Dict().Set("bookmarks", kOverrideResource2));
  scoped_refptr<const Extension> ext_component(
      ExtensionBuilder()
          .SetManifest(std::move(manifest2))
          .SetLocation(ManifestLocation::kComponent)
          .SetID("bbabcdefghijabcdefghijabcdefghij")
          .Build());
  registrar()->AddComponentExtension(ext_component.get());

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

TEST_F(ExtensionWebUITest, OverrideRegistrarObserver) {
  TestOverrideRegistrarObserver observer;
  ExtensionWebUIOverrideRegistrar* override_registrar =
      ExtensionWebUIOverrideRegistrar::GetFactoryInstance()->Get(
          profile_.get());
  override_registrar->AddObserver(&observer);

  scoped_refptr<const Extension> extension(
      ExtensionBuilder("ext1")
          .SetManifestKey(
              api::chrome_url_overrides::ManifestKeys::kChromeUrlOverrides,
              base::Value::Dict().Set("bookmarks", "1.html"))
          .Build());
  const ExtensionId id = extension->id();

  registrar()->AddExtension(extension.get());

  ASSERT_EQ(1u, observer.loaded_extensions().size());
  EXPECT_EQ(id, observer.loaded_extensions()[0]);
  EXPECT_TRUE(observer.unloaded_extensions().empty());

  registrar()->DisableExtension(id, {disable_reason::DISABLE_USER_ACTION});

  ASSERT_EQ(1u, observer.loaded_extensions().size());
  ASSERT_EQ(1u, observer.unloaded_extensions().size());
  EXPECT_EQ(id, observer.unloaded_extensions()[0]);
  override_registrar->RemoveObserver(&observer);
}

TEST_F(ExtensionWebUITest, OverrideRegistrarObserverNoOverride) {
  TestOverrideRegistrarObserver observer;
  ExtensionWebUIOverrideRegistrar* override_registrar =
      ExtensionWebUIOverrideRegistrar::GetFactoryInstance()->Get(
          profile_.get());
  override_registrar->AddObserver(&observer);

  scoped_refptr<const Extension> extension(
      ExtensionBuilder("No override").Build());

  registrar()->AddExtension(extension.get());

  EXPECT_TRUE(observer.loaded_extensions().empty());
  EXPECT_TRUE(observer.unloaded_extensions().empty());

  registrar()->DisableExtension(extension->id(),
                                {disable_reason::DISABLE_USER_ACTION});

  EXPECT_TRUE(observer.loaded_extensions().empty());
  EXPECT_TRUE(observer.unloaded_extensions().empty());
  override_registrar->RemoveObserver(&observer);
}

TEST_F(ExtensionWebUITest, TestRemovingDuplicateEntriesForHosts) {
  // Test that duplicate entries for a single extension are removed. This could
  // happen because of https://crbug.com/782959.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .SetManifestPath("chrome_url_overrides.newtab", "newtab.html")
          .Build();

  const GURL newtab_url = extension->GetResourceURL("newtab.html");

  PrefService* prefs = profile_->GetPrefs();
  {
    // Add multiple entries for the same extension.
    ScopedDictPrefUpdate update(prefs, ExtensionWebUI::kExtensionURLOverrides);
    base::Value::Dict& all_overrides = update.Get();

    auto newtab_list =
        base::Value::List()
            .Append(base::Value::Dict()
                        .Set("entry", newtab_url.spec())
                        .Set("active", true))
            .Append(base::Value::Dict()
                        .Set("entry",
                             extension->GetResourceURL("oldtab.html").spec())
                        .Set("active", true));

    all_overrides.Set("newtab", std::move(newtab_list));
  }

  registrar()->AddExtension(extension.get());
  static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile_.get()))
      ->SetReady();
  base::RunLoop().RunUntilIdle();

  // Duplicates should be removed (in response to ExtensionSystem::ready()).
  // Only a single entry should remain.
  const base::Value::Dict& overrides =
      prefs->GetDict(ExtensionWebUI::kExtensionURLOverrides);
  const base::Value::List* newtab_overrides = overrides.FindList("newtab");
  ASSERT_TRUE(newtab_overrides);
  ASSERT_EQ(1u, newtab_overrides->size());
  const base::Value::Dict& override_dict = (*newtab_overrides)[0].GetDict();
  EXPECT_EQ(newtab_url.spec(), CHECK_DEREF(override_dict.FindString("entry")));
  EXPECT_TRUE(override_dict.FindBool("active").value_or(false));
}

TEST_F(ExtensionWebUITest, TestFaviconAlwaysAvailable) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").Build();
  registrar()->AddExtension(extension.get());
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
      base::BindOnce(set_favicon_results, &favicon_results,
                     run_loop.QuitClosure()));

  run_loop.Run();
  EXPECT_FALSE(favicon_results.empty());

  // Verify that the favicon bitmaps are not empty and are valid.
  for (const auto& favicon : favicon_results) {
    EXPECT_TRUE(favicon.is_valid());

    SkBitmap bitmap = gfx::PNGCodec::Decode(*favicon.bitmap_data);
    EXPECT_FALSE(bitmap.isNull());
    EXPECT_FALSE(bitmap.drawsNothing());
  }
}

TEST_F(ExtensionWebUITest, TestNumExtensionsOverridingURL) {
  auto load_extension_overriding_newtab = [this](const char* name) {
    base::Value::Dict chrome_url_overrides =
        base::Value::Dict().Set("newtab", "newtab.html");
    scoped_refptr<const Extension> extension =
        ExtensionBuilder(name)
            .SetLocation(ManifestLocation::kInternal)
            .SetManifestKey("chrome_url_overrides",
                            std::move(chrome_url_overrides))
            .Build();

    registrar()->AddExtension(extension.get());
    EXPECT_EQ(extension, ExtensionWebUI::GetExtensionControllingURL(
                             GURL(chrome::kChromeUINewTabURL), profile_.get()));

    return extension.get();
  };

  const GURL ntp_url(chrome::kChromeUINewTabURL);

  // Load a series of extensions that override the new tab page.
  const Extension* extension1 = load_extension_overriding_newtab("one");
  ASSERT_TRUE(extension1);
  EXPECT_EQ(1u, ExtensionWebUI::GetNumberOfExtensionsOverridingURL(
                    ntp_url, profile_.get()));

  const Extension* extension2 = load_extension_overriding_newtab("two");
  ASSERT_TRUE(extension2);
  EXPECT_EQ(2u, ExtensionWebUI::GetNumberOfExtensionsOverridingURL(
                    ntp_url, profile_.get()));

  const Extension* extension3 = load_extension_overriding_newtab("three");
  ASSERT_TRUE(extension3);
  EXPECT_EQ(3u, ExtensionWebUI::GetNumberOfExtensionsOverridingURL(
                    ntp_url, profile_.get()));

  // Disabling an extension should remove it from the override count.
  registrar()->DisableExtension(extension2->id(),
                                {disable_reason::DISABLE_USER_ACTION});
  EXPECT_EQ(2u, ExtensionWebUI::GetNumberOfExtensionsOverridingURL(
                    ntp_url, profile_.get()));
}

class ExtensionWebUIOverrideURLTest : public ExtensionServiceTestWithInstall {
 public:
  ExtensionWebUIOverrideURLTest() = default;

  ExtensionWebUIOverrideURLTest(const ExtensionWebUIOverrideURLTest&) = delete;
  ExtensionWebUIOverrideURLTest& operator=(
      const ExtensionWebUIOverrideURLTest&) = delete;

  void SetUp() override;
};

void ExtensionWebUIOverrideURLTest::SetUp() {
  ExtensionServiceTestWithInstall::SetUp();
  InitializeEmptyExtensionService();
}

TEST_F(ExtensionWebUIOverrideURLTest,
       TestUninstallOfURLOverridingExtensionWithoutLoad) {
  FeatureSwitch::ScopedOverride external_prompt_override(
      FeatureSwitch::prompt_for_external_extensions(), true);

  base::FilePath crx_path =
      temp_dir().GetPath().AppendASCII("ntp_override.crx");
  PackCRX(data_dir().AppendASCII("ntp_override"),
          data_dir().AppendASCII("ntp_override.pem"), crx_path);

  ExternalProviderManager* external_provider_manager =
      ExternalProviderManager::Get(profile());
  auto external_provider = std::make_unique<MockExternalProvider>(
      external_provider_manager, ManifestLocation::kExternalPref);
  external_provider->UpdateOrAddExtension(kNtpOverrideExtensionId, "1",
                                          crx_path);
  external_provider_manager->AddProviderForTesting(
      std::move(external_provider));

  TestExtensionRegistryObserver observer(registry(), kNtpOverrideExtensionId);
  external_provider_manager->CheckForExternalUpdates();
  ASSERT_TRUE(observer.WaitForExtensionInstalled());

  // Extension should be disabled by default with right reason.
  EXPECT_TRUE(
      registry()->disabled_extensions().Contains(kNtpOverrideExtensionId));
  EXPECT_FALSE(
      registry()->enabled_extensions().Contains(kNtpOverrideExtensionId));
  EXPECT_THAT(ExtensionPrefs::Get(profile())->GetDisableReasons(
                  kNtpOverrideExtensionId),
              testing::UnorderedElementsAre(
                  disable_reason::DISABLE_EXTERNAL_EXTENSION));

  // URLOverrides pref should not be updated for disabled by default extension.
  PrefService* prefs = profile()->GetPrefs();
  const base::Value::Dict& overrides =
      prefs->GetDict(ExtensionWebUI::kExtensionURLOverrides);
  const base::Value::List* newtab_overrides = overrides.FindList("newtab");
  EXPECT_FALSE(newtab_overrides);

  EXPECT_TRUE(registrar()->UninstallExtension(
      kNtpOverrideExtensionId, UNINSTALL_REASON_FOR_TESTING, nullptr));
  ASSERT_FALSE(registry()->GetInstalledExtension(kNtpOverrideExtensionId));
}

}  // namespace extensions
