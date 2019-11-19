// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/component_loader.h"

#include <stddef.h>

#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class MockExtensionService : public TestExtensionService {
 private:
  bool ready_;
  size_t unloaded_count_;
  ExtensionRegistry* registry_;

 public:
  explicit MockExtensionService(Profile* profile)
      : ready_(false),
        unloaded_count_(0),
        registry_(ExtensionRegistry::Get(profile)) {}

  void AddComponentExtension(const Extension* extension) override {
    EXPECT_FALSE(registry_->enabled_extensions().Contains(extension->id()));
    // ExtensionService must become the owner of the extension object.
    registry_->AddEnabled(extension);
  }

  void UnloadExtension(const std::string& extension_id,
                       UnloadedExtensionReason reason) override {
    ASSERT_TRUE(registry_->enabled_extensions().Contains(extension_id));
    // Remove the extension with the matching id.
    registry_->RemoveEnabled(extension_id);
    unloaded_count_++;
  }

  void RemoveComponentExtension(const std::string& extension_id) override {
    UnloadExtension(extension_id, UnloadedExtensionReason::DISABLE);
  }

  bool is_ready() override { return ready_; }

  void set_ready(bool ready) {
    ready_ = ready;
  }

  size_t unloaded_count() const {
    return unloaded_count_;
  }

  void clear_extensions() { registry_->ClearAll(); }
};

}  // namespace

class ComponentLoaderTest : public testing::Test {
 public:
  ComponentLoaderTest()
      : extension_service_(&profile_),
        component_loader_(&extension_service_,
                          &profile_) {
    component_loader_.set_ignore_whitelist_for_testing(true);
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
  MockExtensionService extension_service_;
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
  std::unique_ptr<base::DictionaryValue> manifest;

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

  int value = 0;
  manifest = component_loader_.ParseManifest(
      "{ \"test\": { \"one\": 1 }, \"two\": 2 }");
  ASSERT_TRUE(manifest);
  EXPECT_TRUE(manifest->GetInteger("test.one", &value));
  EXPECT_EQ(1, value);
  ASSERT_TRUE(manifest->GetInteger("two", &value));
  EXPECT_EQ(2, value);

  std::string string_value;
  manifest = component_loader_.ParseManifest(manifest_contents_);
  ASSERT_TRUE(manifest->GetString("background.page", &string_value));
  EXPECT_EQ("backgroundpage.html", string_value);
}

// Test that the extension isn't loaded if the extension service isn't ready.
TEST_F(ComponentLoaderTest, AddWhenNotReady) {
  extension_service_.set_ready(false);
  std::string extension_id =
      component_loader_.Add(manifest_contents_, extension_path_);
  EXPECT_NE("", extension_id);
  ExtensionRegistry* registry = ExtensionRegistry::Get(&profile_);
  EXPECT_EQ(0u, registry->enabled_extensions().size());
}

// Test that it *is* loaded when the extension service *is* ready.
TEST_F(ComponentLoaderTest, AddWhenReady) {
  extension_service_.set_ready(true);
  std::string extension_id =
      component_loader_.Add(manifest_contents_, extension_path_);
  EXPECT_NE("", extension_id);
  ExtensionRegistry* registry = ExtensionRegistry::Get(&profile_);
  EXPECT_EQ(1u, registry->enabled_extensions().size());
  EXPECT_TRUE(registry->enabled_extensions().GetByID(extension_id));
}

TEST_F(ComponentLoaderTest, Remove) {
  extension_service_.set_ready(false);
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
  extension_service_.set_ready(true);
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
  extension_service_.set_ready(false);
  ExtensionRegistry* registry = ExtensionRegistry::Get(&profile_);

  // No extensions should be loaded if none were added.
  component_loader_.LoadAll();
  EXPECT_EQ(0u, registry->enabled_extensions().size());

  // Use LoadAll() to load the default extensions.
  component_loader_.AddDefaultComponentExtensions(false);
  component_loader_.LoadAll();
  unsigned int default_count = registry->enabled_extensions().size();

  // Clear the list of loaded extensions, and reload with one more.
  extension_service_.clear_extensions();
  component_loader_.Add(manifest_contents_, extension_path_);
  component_loader_.LoadAll();

  EXPECT_EQ(default_count + 1, registry->enabled_extensions().size());
}

TEST_F(ComponentLoaderTest, AddOrReplace) {
  EXPECT_EQ(0u, component_loader_.registered_extensions_count());
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

  extension_service_.set_ready(true);
  component_loader_.LoadAll();
  ExtensionRegistry* registry = ExtensionRegistry::Get(&profile_);

  EXPECT_EQ(default_count + 1, registry->enabled_extensions().size());
  EXPECT_EQ(0u, extension_service_.unloaded_count());

  // replace loaded component extension.
  component_loader_.AddOrReplace(known_extension);
  EXPECT_EQ(default_count + 1, registry->enabled_extensions().size());
  EXPECT_EQ(1u, extension_service_.unloaded_count());

  // Add an invalid component extension.
  std::string extension_id = component_loader_.AddOrReplace(invalid_extension);
  EXPECT_TRUE(extension_id.empty());
}

}  // namespace extensions
