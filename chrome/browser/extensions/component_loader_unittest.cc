// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/component_loader.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_user_test_base.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
class ExtensionUnloadedObserver : public ExtensionRegistryObserver {
 public:
  explicit ExtensionUnloadedObserver(ExtensionRegistry* registry)
      : unloaded_count_(0) {
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
  size_t unloaded_count_;
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      observation_{this};
};

class ComponentLoaderTest : public ExtensionServiceUserTestBase {
 public:
  void SetUp() override {
    ExtensionServiceUserTestBase::InitializeEmptyExtensionService();
    ExtensionServiceUserTestBase::SetUp();
    extension_system_ = static_cast<TestExtensionSystem*>(
        ExtensionSystem::Get(testing_profile()));

    extension_path_ =
        GetBasePath().AppendASCII("good")
                     .AppendASCII("Extensions")
                     .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                     .AppendASCII("1.0.0.0");

    // Read in the extension manifest.
    ASSERT_TRUE(base::ReadFileToString(
        extension_path_.Append(kManifestFilename),
        &manifest_contents_));
  }

 protected:
  raw_ptr<TestExtensionSystem> extension_system_;

  // The root directory of the text extension.
  base::FilePath extension_path_;

  // The contents of the text extension's manifest file.
  std::string manifest_contents_;

  base::FilePath GetBasePath() {
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    return test_data_dir.AppendASCII("extensions");
  }

  // Test that certain histograms are emitted for user and non-user profiles
  // (users for ChromeOS Ash).
  void RunEmitUserHistogramsTest(int nonuser_expected_total_count,
                                 int user_expected_total_count) {
    service_->component_loader()->set_profile_for_testing(testing_profile());
    base::HistogramTester histograms;
    service_->component_loader()->LoadAll();
    histograms.ExpectTotalCount("Extensions.LoadAllComponentTime", 1);
    histograms.ExpectTotalCount("Extensions.LoadAllComponentTime.NonUser",
                                nonuser_expected_total_count);
    histograms.ExpectTotalCount("Extensions.LoadAllComponentTime.User",
                                user_expected_total_count);
  }
};

TEST_F(ComponentLoaderTest, ParseManifest) {
  std::optional<base::Value::Dict> manifest;

  // Test invalid JSON.
  manifest =
      service_->component_loader()->ParseManifest("{ 'test': 3 } invalid");
  EXPECT_FALSE(manifest);

  // Test manifests that are valid JSON, but don't have an object literal
  // at the root. ParseManifest() should always return NULL.

  manifest = service_->component_loader()->ParseManifest(std::string());
  EXPECT_FALSE(manifest);

  manifest = service_->component_loader()->ParseManifest("[{ \"foo\": 3 }]");
  EXPECT_FALSE(manifest);

  manifest = service_->component_loader()->ParseManifest("\"Test\"");
  EXPECT_FALSE(manifest);

  manifest = service_->component_loader()->ParseManifest("42");
  EXPECT_FALSE(manifest);

  manifest = service_->component_loader()->ParseManifest("true");
  EXPECT_FALSE(manifest);

  manifest = service_->component_loader()->ParseManifest("false");
  EXPECT_FALSE(manifest);

  manifest = service_->component_loader()->ParseManifest("null");
  EXPECT_FALSE(manifest);

  // Test parsing valid JSON.

  manifest = service_->component_loader()->ParseManifest(
      "{ \"test\": { \"one\": 1 }, \"two\": 2 }");
  ASSERT_TRUE(manifest);
  EXPECT_EQ(1, manifest->FindIntByDottedPath("test.one"));
  EXPECT_EQ(2, manifest->FindInt("two"));

  manifest = service_->component_loader()->ParseManifest(manifest_contents_);
  const std::string* string_value =
      manifest->FindStringByDottedPath("background.page");
  ASSERT_TRUE(string_value);
  EXPECT_EQ("backgroundpage.html", *string_value);
}

// Test that the extension isn't loaded if the extension service isn't ready.
TEST_F(ComponentLoaderTest, AddWhenNotReady) {
  std::string extension_id =
      service_->component_loader()->Add(manifest_contents_, extension_path_);
  EXPECT_NE("", extension_id);
  ExtensionRegistry* registry = ExtensionRegistry::Get(testing_profile());
  EXPECT_EQ(0u, registry->enabled_extensions().size());
}

// Test that it *is* loaded when the extension service *is* ready.
TEST_F(ComponentLoaderTest, AddWhenReady) {
  extension_system_->SetReady();
  std::string extension_id =
      service_->component_loader()->Add(manifest_contents_, extension_path_);
  EXPECT_NE("", extension_id);
  ExtensionRegistry* registry = ExtensionRegistry::Get(testing_profile());
  EXPECT_EQ(1u, registry->enabled_extensions().size());
  EXPECT_TRUE(registry->enabled_extensions().GetByID(extension_id));
}

TEST_F(ComponentLoaderTest, Remove) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(testing_profile());

  // Removing an extension that was never added should be ok.
  service_->component_loader()->Remove(extension_path_);
  EXPECT_EQ(0u, registry->enabled_extensions().size());

  // Try adding and removing before LoadAll() is called.
  service_->component_loader()->Add(manifest_contents_, extension_path_);
  service_->component_loader()->Remove(extension_path_);
  service_->component_loader()->LoadAll();
  EXPECT_EQ(0u, registry->enabled_extensions().size());

  // Load an extension, and check that it's unloaded when Remove() is called.
  extension_system_->SetReady();
  std::string extension_id =
      service_->component_loader()->Add(manifest_contents_, extension_path_);
  EXPECT_EQ(1u, registry->enabled_extensions().size());
  service_->component_loader()->Remove(extension_path_);
  EXPECT_EQ(0u, registry->enabled_extensions().size());

  // And after calling LoadAll(), it shouldn't get loaded.
  service_->component_loader()->LoadAll();
  EXPECT_EQ(0u, registry->enabled_extensions().size());
}

TEST_F(ComponentLoaderTest, LoadAll) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(testing_profile());

  // No extensions should be loaded if none were added.
  service_->component_loader()->LoadAll();
  EXPECT_EQ(0u, registry->enabled_extensions().size());

  // Use LoadAll() to load the default extensions.
  service_->component_loader()->AddDefaultComponentExtensions(false);
  service_->component_loader()->LoadAll();
  unsigned int default_count = registry->enabled_extensions().size();

  // Clear the list of loaded extensions, and reload with one more.
  extension_system_->extension_service()->UnloadAllExtensionsForTest();
  service_->component_loader()->Add(manifest_contents_, extension_path_);
  service_->component_loader()->LoadAll();

  EXPECT_EQ(default_count + 1, registry->enabled_extensions().size());
}

TEST_F(ComponentLoaderTest, LoadAll_EmitUserHistograms) {
  ASSERT_NO_FATAL_FAILURE(MaybeSetUpTestUser(/*is_guest=*/false));

  RunEmitUserHistogramsTest(/*nonuser_expected_total_count=*/0,
                            /*user_expected_total_count=*/1);
}

TEST_F(ComponentLoaderTest, LoadAll_NonUserEmitHistograms) {
  ASSERT_NO_FATAL_FAILURE(MaybeSetUpTestUser(/*is_guest=*/true));

  RunEmitUserHistogramsTest(/*nonuser_expected_total_count=*/1,
                            /*user_expected_total_count=*/0);
}

// Test is flaky. https://crbug.com/1306983
TEST_F(ComponentLoaderTest, DISABLED_AddOrReplace) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(testing_profile());
  ExtensionUnloadedObserver unload_observer(registry);
  EXPECT_EQ(0u, service_->component_loader()->registered_extensions_count());

  // Allow the Feedback extension, which has a background page, to be loaded.
  service_->component_loader()->EnableBackgroundExtensionsForTesting();

  service_->component_loader()->AddDefaultComponentExtensions(false);
  size_t const default_count =
      service_->component_loader()->registered_extensions_count();
  base::FilePath known_extension = GetBasePath()
      .AppendASCII("override_component_extension");
  base::FilePath unknown_extension = extension_path_;
  base::FilePath invalid_extension = GetBasePath()
      .AppendASCII("this_path_does_not_exist");

  // Replace a default component extension.
  service_->component_loader()->AddOrReplace(known_extension);
  EXPECT_EQ(default_count,
            service_->component_loader()->registered_extensions_count());

  // Add a new component extension.
  service_->component_loader()->AddOrReplace(unknown_extension);
  EXPECT_EQ(default_count + 1,
            service_->component_loader()->registered_extensions_count());

  extension_system_->SetReady();
  service_->component_loader()->LoadAll();

  EXPECT_EQ(default_count + 1, registry->enabled_extensions().size());
  EXPECT_EQ(0u, unload_observer.unloaded_count());

  // replace loaded component extension.
  service_->component_loader()->AddOrReplace(known_extension);
  EXPECT_EQ(default_count + 1, registry->enabled_extensions().size());
  EXPECT_EQ(1u, unload_observer.unloaded_count());

  // Add an invalid component extension.
  std::string extension_id =
      service_->component_loader()->AddOrReplace(invalid_extension);
  EXPECT_TRUE(extension_id.empty());
}

}  // namespace extensions
