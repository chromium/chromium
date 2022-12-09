// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/component_loader.h"

#include <stddef.h>

#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"

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

class ComponentLoaderTest : public testing::Test {
 public:
  ComponentLoaderTest()
      : extension_system_(
            static_cast<TestExtensionSystem*>(ExtensionSystem::Get(&profile_))),
        component_loader_(extension_system_, &profile_) {
    extension_system_->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(),
        base::FilePath() /* install_directory */,
        false /* autoupdate_enabled */);
    component_loader_.set_ignore_allowlist_for_testing(true);
  }

  void SetUp() override {
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
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  raw_ptr<TestExtensionSystem> extension_system_;
  ComponentLoader component_loader_;

  // The root directory of the text extension.
  base::FilePath extension_path_;

  // The contents of the text extension's manifest file.
  std::string manifest_contents_;

  base::FilePath GetBasePath() {
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    return test_data_dir.AppendASCII("extensions");
  }
};

TEST_F(ComponentLoaderTest, ParseManifest) {
  absl::optional<base::Value::Dict> manifest;

  // Test invalid JSON.
  manifest = component_loader_.ParseManifest("{ 'test': 3 } invalid");
  EXPECT_FALSE(manifest);

  // Test manifests that are valid JSON, but don't have an object literal
  // at the root. ParseManifest() should always return NULL.

  manifest = component_loader_.ParseManifest(std::string());
  EXPECT_FALSE(manifest);

  manifest = component_loader_.ParseManifest("[{ \"foo\": 3 }]");
  EXPECT_FALSE(manifest);

  manifest = component_loader_.ParseManifest("\"Test\"");
  EXPECT_FALSE(manifest);

  manifest = component_loader_.ParseManifest("42");
  EXPECT_FALSE(manifest);

  manifest = component_loader_.ParseManifest("true");
  EXPECT_FALSE(manifest);

  manifest = component_loader_.ParseManifest("false");
  EXPECT_FALSE(manifest);

  manifest = component_loader_.ParseManifest("null");
  EXPECT_FALSE(manifest);

  // Test parsing valid JSON.

  manifest = component_loader_.ParseManifest(
      "{ \"test\": { \"one\": 1 }, \"two\": 2 }");
  ASSERT_TRUE(manifest);
  EXPECT_EQ(1, manifest->FindIntByDottedPath("test.one"));
  EXPECT_EQ(2, manifest->FindInt("two"));

  manifest = component_loader_.ParseManifest(manifest_contents_);
  const std::string* string_value =
      manifest->FindStringByDottedPath("background.page");
  ASSERT_TRUE(string_value);
  EXPECT_EQ("backgroundpage.html", *string_value);
}

// Test that the extension isn't loaded if the extension service isn't ready.
TEST_F(ComponentLoaderTest, AddWhenNotReady) {
  std::string extension_id =
      component_loader_.Add(manifest_contents_, extension_path_);
  EXPECT_NE("", extension_id);
  ExtensionRegistry* registry = ExtensionRegistry::Get(&profile_);
  EXPECT_EQ(0u, registry->enabled_extensions().size());
}

// Test that it *is* loaded when the extension service *is* ready.
TEST_F(ComponentLoaderTest, AddWhenReady) {
  extension_system_->SetReady();
  std::string extension_id =
      component_loader_.Add(manifest_contents_, extension_path_);
  EXPECT_NE("", extension_id);
  ExtensionRegistry* registry = ExtensionRegistry::Get(&profile_);
  EXPECT_EQ(1u, registry->enabled_extensions().size());
  EXPECT_TRUE(registry->enabled_extensions().GetByID(extension_id));
}

TEST_F(ComponentLoaderTest, Remove) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(&profile_);

  // Removing an extension that was never added should be ok.
  component_loader_.Remove(extension_path_);
  EXPECT_EQ(0u, registry->enabled_extensions().size());

  // Try adding and removing before LoadAll() is called.
  component_loader_.Add(manifest_contents_, extension_path_);
  component_loader_.Remove(extension_path_);
  component_loader_.LoadAll();
  EXPECT_EQ(0u, registry->enabled_extensions().size());

  // Load an extension, and check that it's unloaded when Remove() is called.
  extension_system_->SetReady();
  std::string extension_id =
      component_loader_.Add(manifest_contents_, extension_path_);
  EXPECT_EQ(1u, registry->enabled_extensions().size());
  component_loader_.Remove(extension_path_);
  EXPECT_EQ(0u, registry->enabled_extensions().size());

  // And after calling LoadAll(), it shouldn't get loaded.
  component_loader_.LoadAll();
  EXPECT_EQ(0u, registry->enabled_extensions().size());
}

TEST_F(ComponentLoaderTest, LoadAll) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(&profile_);

  // No extensions should be loaded if none were added.
  component_loader_.LoadAll();
  EXPECT_EQ(0u, registry->enabled_extensions().size());

  // Use LoadAll() to load the default extensions.
  component_loader_.AddDefaultComponentExtensions(false);
  component_loader_.LoadAll();
  unsigned int default_count = registry->enabled_extensions().size();

  // Clear the list of loaded extensions, and reload with one more.
  extension_system_->extension_service()->UnloadAllExtensionsForTest();
  component_loader_.Add(manifest_contents_, extension_path_);
  component_loader_.LoadAll();

  EXPECT_EQ(default_count + 1, registry->enabled_extensions().size());
}

// Test is flaky. https://crbug.com/1306983
TEST_F(ComponentLoaderTest, DISABLED_AddOrReplace) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(&profile_);
  ExtensionUnloadedObserver unload_observer(registry);
  EXPECT_EQ(0u, component_loader_.registered_extensions_count());

  // Allow the Feedback extension, which has a background page, to be loaded.
  component_loader_.EnableBackgroundExtensionsForTesting();

  component_loader_.AddDefaultComponentExtensions(false);
  size_t const default_count = component_loader_.registered_extensions_count();
  base::FilePath known_extension = GetBasePath()
      .AppendASCII("override_component_extension");
  base::FilePath unknown_extension = extension_path_;
  base::FilePath invalid_extension = GetBasePath()
      .AppendASCII("this_path_does_not_exist");

  // Replace a default component extension.
  component_loader_.AddOrReplace(known_extension);
  EXPECT_EQ(default_count, component_loader_.registered_extensions_count());

  // Add a new component extension.
  component_loader_.AddOrReplace(unknown_extension);
  EXPECT_EQ(default_count + 1, component_loader_.registered_extensions_count());

  extension_system_->SetReady();
  component_loader_.LoadAll();

  EXPECT_EQ(default_count + 1, registry->enabled_extensions().size());
  EXPECT_EQ(0u, unload_observer.unloaded_count());

  // replace loaded component extension.
  component_loader_.AddOrReplace(known_extension);
  EXPECT_EQ(default_count + 1, registry->enabled_extensions().size());
  EXPECT_EQ(1u, unload_observer.unloaded_count());

  // Add an invalid component extension.
  std::string extension_id = component_loader_.AddOrReplace(invalid_extension);
  EXPECT_TRUE(extension_id.empty());
}

}  // namespace extensions
