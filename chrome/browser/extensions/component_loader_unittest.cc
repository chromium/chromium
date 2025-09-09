// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/component_loader.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/one_shot_event.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_extension_registrar_delegate.h"
#include "chrome/browser/extensions/extension_service_user_test_base.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/test_screen.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
class ExtensionUnloadedObserver : public ExtensionRegistryObserver {
 public:
  explicit ExtensionUnloadedObserver(ExtensionRegistry* registry) {
    observation_.Observe(registry);
  }

  ExtensionUnloadedObserver(const ExtensionUnloadedObserver&) = delete;
  ExtensionUnloadedObserver& operator=(const ExtensionUnloadedObserver&) =
      delete;

  size_t unloaded_count() const { return unloaded_count_; }

 protected:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override {
    ASSERT_TRUE(Manifest::IsComponentLocation(extension->location()));
    ++unloaded_count_;
  }

 private:
  size_t unloaded_count_ = 0;
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      observation_{this};
};

// TODO(crbug.com/408458901): Use an extensions test base class once we have
// one that works on desktop Android.
class ComponentLoaderTest : public testing::Test {
 public:
  ComponentLoaderTest() = default;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile(
        TestingProfile::kDefaultProfileUserName, /*prefs=*/nullptr,
        /*user_name=*/std::u16string(),
        /*avatar_id=*/0, /*testing_factories=*/{});

    extension_system_ =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()));

    extension_path_ =
        GetBasePath().AppendASCII("good")
                     .AppendASCII("Extensions")
                     .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                     .AppendASCII("1.0.0.0");

    // Read in the extension manifest.
    ASSERT_TRUE(base::ReadFileToString(
        extension_path_.Append(kManifestFilename),
        &manifest_contents_));

    component_loader_ = ComponentLoader::Get(profile());
    component_loader_->set_ignore_allowlist_for_testing(true);

    // Set up ExtensionRegistrar with a delegate.
    extension_registrar_delegate_ =
        std::make_unique<ChromeExtensionRegistrarDelegate>(profile_.get());
    extension_registrar_ = ExtensionRegistrar::Get(profile_.get());
    extension_registrar_->Init(extension_registrar_delegate_.get(), true,
                               base::CommandLine::ForCurrentProcess(),
                               base::FilePath(), base::FilePath());
    extension_registrar_delegate_->Init(extension_registrar_);
  }

  void TearDown() override {
    extension_registrar_delegate_->Shutdown();
    extension_registrar_ = nullptr;
    component_loader_ = nullptr;
    extension_system_ = nullptr;
    profile_ = nullptr;
    profile_manager_.reset();
  }

  base::FilePath GetBasePath() {
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    return test_data_dir.AppendASCII("extensions");
  }

  void UninstallAllExtensions() {
    auto* registry = ExtensionRegistry::Get(profile());
    ExtensionSet extensions = registry->GenerateInstalledExtensionsSet();
    for (const auto& extension : extensions) {
      extension_registrar_->RemoveExtension(extension->id(),
                                            UnloadedExtensionReason::UNINSTALL);
    }
  }

  TestingProfile* profile() { return profile_; }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  std::unique_ptr<ChromeExtensionRegistrarDelegate>
      extension_registrar_delegate_;
  raw_ptr<ExtensionRegistrar> extension_registrar_ = nullptr;
  raw_ptr<TestExtensionSystem> extension_system_ = nullptr;
  raw_ptr<ComponentLoader> component_loader_ = nullptr;

  // The root directory of the text extension.
  base::FilePath extension_path_;

  // The contents of the text extension's manifest file.
  std::string manifest_contents_;

#if BUILDFLAG(IS_ANDROID)
  // WebContentImpl requires a Screen instance on Android.
  display::test::TestScreen screen_{/*create_display=*/true,
                                    /*register_screen=*/true};
#endif
};

TEST_F(ComponentLoaderTest, ParseManifest) {
  std::optional<base::Value::Dict> manifest;

  // Test invalid JSON.
  manifest = component_loader_->ParseManifest("{ 'test': 3 } invalid");
  EXPECT_FALSE(manifest);

  // Test manifests that are valid JSON, but don't have an object literal
  // at the root. ParseManifest() should always return NULL.

  manifest = component_loader_->ParseManifest(std::string());
  EXPECT_FALSE(manifest);

  manifest = component_loader_->ParseManifest("[{ \"foo\": 3 }]");
  EXPECT_FALSE(manifest);

  manifest = component_loader_->ParseManifest("\"Test\"");
  EXPECT_FALSE(manifest);

  manifest = component_loader_->ParseManifest("42");
  EXPECT_FALSE(manifest);

  manifest = component_loader_->ParseManifest("true");
  EXPECT_FALSE(manifest);

  manifest = component_loader_->ParseManifest("false");
  EXPECT_FALSE(manifest);

  manifest = component_loader_->ParseManifest("null");
  EXPECT_FALSE(manifest);

  // Test parsing valid JSON.

  manifest = component_loader_->ParseManifest(
      "{ \"test\": { \"one\": 1 }, \"two\": 2 }");
  ASSERT_TRUE(manifest);
  EXPECT_EQ(1, manifest->FindIntByDottedPath("test.one"));
  EXPECT_EQ(2, manifest->FindInt("two"));

  manifest = component_loader_->ParseManifest(manifest_contents_);
  const std::string* string_value =
      manifest->FindStringByDottedPath("background.page");
  ASSERT_TRUE(string_value);
  EXPECT_EQ("backgroundpage.html", *string_value);
}

// Test that the extension isn't loaded if the extension service isn't ready.
TEST_F(ComponentLoaderTest, AddWhenNotReady) {
  std::string extension_id =
      component_loader_->Add(manifest_contents_, extension_path_);
  EXPECT_NE("", extension_id);
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  EXPECT_EQ(0u, registry->enabled_extensions().size());
}

// Test that it *is* loaded when the extension service *is* ready.
TEST_F(ComponentLoaderTest, AddWhenReady) {
  extension_system_->SetReady();
  std::string extension_id =
      component_loader_->Add(manifest_contents_, extension_path_);
  EXPECT_NE("", extension_id);
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  EXPECT_EQ(1u, registry->enabled_extensions().size());
  EXPECT_TRUE(registry->enabled_extensions().GetByID(extension_id));
}

TEST_F(ComponentLoaderTest, Remove) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());

  // Removing an extension that was never added should be ok.
  component_loader_->Remove(extension_path_);
  EXPECT_EQ(0u, registry->enabled_extensions().size());

  // Try adding and removing before LoadAll() is called.
  component_loader_->Add(manifest_contents_, extension_path_);
  component_loader_->Remove(extension_path_);
  component_loader_->LoadAll();
  EXPECT_EQ(0u, registry->enabled_extensions().size());

  // Load an extension, and check that it's unloaded when Remove() is called.
  extension_system_->SetReady();
  std::string extension_id =
      component_loader_->Add(manifest_contents_, extension_path_);
  EXPECT_EQ(1u, registry->enabled_extensions().size());
  component_loader_->Remove(extension_path_);
  EXPECT_EQ(0u, registry->enabled_extensions().size());

  // And after calling LoadAll(), it shouldn't get loaded.
  component_loader_->LoadAll();
  EXPECT_EQ(0u, registry->enabled_extensions().size());
}

TEST_F(ComponentLoaderTest, LoadAll) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());

  // No extensions should be loaded if none were added.
  component_loader_->LoadAll();
  EXPECT_EQ(0u, registry->enabled_extensions().size());

  // Use LoadAll() to load the default extensions.
  component_loader_->AddDefaultComponentExtensions(false);
  component_loader_->LoadAll();
  unsigned int default_count = registry->enabled_extensions().size();

  // Clear the list of loaded extensions, and reload with one more.
  UninstallAllExtensions();
  component_loader_->Add(manifest_contents_, extension_path_);
  component_loader_->LoadAll();

  EXPECT_EQ(default_count + 1, registry->enabled_extensions().size());
}

class ComponentLoaderExtensionServiceTest
    : public ExtensionServiceUserTestBase {
 public:
  // testing::Test:
  void SetUp() override {
    ExtensionServiceUserTestBase::InitializeEmptyExtensionService();
    ExtensionServiceUserTestBase::SetUp();
  }

  // Test that certain histograms are emitted for user and non-user profiles
  // (users for ChromeOS Ash).
  void RunEmitUserHistogramsTest(int nonuser_expected_total_count,
                                 int user_expected_total_count) {
    auto* component_loader = ComponentLoader::Get(profile());
    component_loader->set_profile_for_testing(profile());
    base::HistogramTester histograms;
    component_loader->LoadAll();
    histograms.ExpectTotalCount("Extensions.LoadAllComponentTime", 1);
    histograms.ExpectTotalCount("Extensions.LoadAllComponentTime.NonUser",
                                nonuser_expected_total_count);
    histograms.ExpectTotalCount("Extensions.LoadAllComponentTime.User",
                                user_expected_total_count);
  }
};

TEST_F(ComponentLoaderExtensionServiceTest, LoadAll_EmitUserHistograms) {
  ASSERT_NO_FATAL_FAILURE(MaybeSetUpTestUser(/*is_guest=*/false));

  RunEmitUserHistogramsTest(/*nonuser_expected_total_count=*/0,
                            /*user_expected_total_count=*/1);
}

TEST_F(ComponentLoaderExtensionServiceTest, LoadAll_NonUserEmitHistograms) {
  ASSERT_NO_FATAL_FAILURE(MaybeSetUpTestUser(/*is_guest=*/true));

  RunEmitUserHistogramsTest(/*nonuser_expected_total_count=*/1,
                            /*user_expected_total_count=*/0);
}

// Test is flaky. https://crbug.com/1306983
TEST_F(ComponentLoaderTest, DISABLED_AddOrReplace) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  ExtensionUnloadedObserver unload_observer(registry);
  EXPECT_EQ(0u, component_loader_->registered_extensions_count());

  // Allow the Feedback extension, which has a background page, to be loaded.
  component_loader_->EnableBackgroundExtensionsForTesting();

  component_loader_->AddDefaultComponentExtensions(false);
  size_t const default_count = component_loader_->registered_extensions_count();
  base::FilePath known_extension = GetBasePath()
      .AppendASCII("override_component_extension");
  base::FilePath unknown_extension = extension_path_;
  base::FilePath invalid_extension = GetBasePath()
      .AppendASCII("this_path_does_not_exist");

  // Replace a default component extension.
  component_loader_->AddOrReplace(known_extension);
  EXPECT_EQ(default_count, component_loader_->registered_extensions_count());

  // Add a new component extension.
  component_loader_->AddOrReplace(unknown_extension);
  EXPECT_EQ(default_count + 1,
            component_loader_->registered_extensions_count());

  extension_system_->SetReady();
  component_loader_->LoadAll();

  EXPECT_EQ(default_count + 1, registry->enabled_extensions().size());
  EXPECT_EQ(0u, unload_observer.unloaded_count());

  // replace loaded component extension.
  component_loader_->AddOrReplace(known_extension);
  EXPECT_EQ(default_count + 1, registry->enabled_extensions().size());
  EXPECT_EQ(1u, unload_observer.unloaded_count());

  // Add an invalid component extension.
  std::string extension_id = component_loader_->AddOrReplace(invalid_extension);
  EXPECT_TRUE(extension_id.empty());
}

#if BUILDFLAG(IS_CHROMEOS)

// Tests that component extensions follow symbolic links for its resources.
TEST_F(ComponentLoaderTest, FollowSymlinks) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const base::FilePath extension_dir =
      temp_dir.GetPath().AppendASCII("extension");
  ASSERT_TRUE(base::CreateDirectory(extension_dir));

  constexpr char kKey[] =
      "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA0wsqv0aItfmI5B5"
      "fTOChbevC1X5Xm/2et2H1TpocRQGSz5oGhVMUPjGgBeYvHemCkBGiuFYQgT"
      "62aPZLNqQG96dFEG4GGuY/FX7Kq1G9NF1ovZ9H5JMniEj/zfj1W64mCwgFI"
      "uVN8b3m+vs3ZoCX9OD4s9mEEEiygqltfsxhGBAmPLdDcY6Ze9/FHNJh6Q6s"
      "Aa6hIhfmmbLSyvX/kL3359J1yVTFuW1SK2/hfDINl+MWkHgd2xV0tkl/bVy"
      "NlH4XtoK3ZedHe7mrkiEoO65U/PpmN/coLoE07C/xkQwYVqbQWllq2Ij8j2"
      "XkJdwv3qrcDsOvvB1W498CyMkuGL2UswIDAQAB";
  constexpr char kManifestTemplate[] =
      R"({
        "key": "%s",
        "version": "1.0",
        "manifest_version": 3,
        "name": "Test extension"
      })";
  ASSERT_TRUE(base::WriteFile(extension_dir.Append(kManifestFilename),
                              base::StringPrintf(kManifestTemplate, kKey)));

  const base::FilePath shared_data = temp_dir.GetPath().AppendASCII("data");
  ASSERT_TRUE(base::WriteFile(shared_data, "shared tests file"));

  const base::FilePath rel_path_in_extension_root(FILE_PATH_LITERAL("link"));
  base::FilePath link_in_extension_root =
      extension_dir.Append(rel_path_in_extension_root);
  ASSERT_TRUE(base::CreateSymbolicLink(shared_data, link_in_extension_root));

  extension_system_->SetReady();
  const ExtensionId extension_id =
      component_loader_->AddOrReplace(extension_dir);
  ASSERT_NE("", extension_id);

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  const Extension* extension =
      registry->enabled_extensions().GetByID(extension_id);
  ASSERT_TRUE(extension);

  const base::FilePath resource_path =
      extension->GetResource(rel_path_in_extension_root).GetFilePath();
  EXPECT_EQ(base::MakeAbsoluteFilePath(resource_path),
            base::MakeAbsoluteFilePath(shared_data));
}

// Tests that extensions added via file task runner are tracked during loading.
TEST_F(ComponentLoaderTest, PendingAdd) {
  const ExtensionId kExtensionId = "behllobkkfkfnphdnhnkndlbkcpglgmj";
  base::RunLoop run_loop;

  EXPECT_FALSE(component_loader_->ExistsOrPendingAdd(kExtensionId));

  // Pass `kManifestFilename` for both regular and guest manifest name. This
  // works around `IsNormalSession` that requires `UserManager` instance to be
  // considered as regular session.
  component_loader_->AddComponentFromDirWithManifestFilename(
      extension_path_, kExtensionId, kManifestFilename, kManifestFilename,
      /*done_cb=*/run_loop.QuitClosure(), /*error_cb=*/run_loop.QuitClosure());
  EXPECT_FALSE(component_loader_->Exists(kExtensionId));
  EXPECT_TRUE(component_loader_->IsPendingAdd(kExtensionId));
  EXPECT_TRUE(component_loader_->ExistsOrPendingAdd(kExtensionId));

  run_loop.Run();
  EXPECT_TRUE(component_loader_->Exists(kExtensionId));
  EXPECT_FALSE(component_loader_->IsPendingAdd(kExtensionId));
  EXPECT_TRUE(component_loader_->ExistsOrPendingAdd(kExtensionId));
}

// Tests that removing a pending add extension cancels the loading.
TEST_F(ComponentLoaderTest, RemovePendingAdd) {
  const ExtensionId kExtensionId = "behllobkkfkfnphdnhnkndlbkcpglgmj";
  base::RunLoop run_loop;

  EXPECT_FALSE(component_loader_->ExistsOrPendingAdd(kExtensionId));

  // Pass `kManifestFilename` for both regular and guest manifest name. This
  // works around `IsNormalSession` that requires `UserManager` instance to be
  // considered as regular session.
  component_loader_->AddComponentFromDirWithManifestFilename(
      extension_path_, kExtensionId, kManifestFilename, kManifestFilename,
      /*done_cb=*/run_loop.QuitClosure(), /*error_cb=*/run_loop.QuitClosure());

  // Remove when the extension is loading.
  EXPECT_TRUE(component_loader_->IsPendingAdd(kExtensionId));
  component_loader_->Remove(kExtensionId);

  run_loop.Run();

  // After loading, the extension is no longer pending and stays unloaded.
  EXPECT_FALSE(component_loader_->Exists(kExtensionId));
  EXPECT_FALSE(component_loader_->IsPendingAdd(kExtensionId));
  EXPECT_FALSE(component_loader_->ExistsOrPendingAdd(kExtensionId));
}

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace extensions
