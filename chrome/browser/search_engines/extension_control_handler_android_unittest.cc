// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/extension_control_handler_android.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

class ExtensionControlHandlerAndroidTest : public ::testing::Test {
 protected:
  // testing::Test:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();

    auto* extension_system = static_cast<extensions::TestExtensionSystem*>(
        extensions::ExtensionSystem::Get(profile_.get()));
    auto* extension_service = extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
    extension_service->Init();

    // Wait for the extension system to be ready.
    base::RunLoop run_loop;
    extension_system->ready().Post(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();

    handler_ = std::make_unique<ExtensionControlHandler>(profile_.get());
  }

  // testing::Test:
  void TearDown() override {
    handler_.reset();
    profile_.reset();
  }

  TestingProfile* profile() { return profile_.get(); }
  ExtensionControlHandler* handler() { return handler_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ExtensionControlHandler> handler_;
};

TEST_F(ExtensionControlHandlerAndroidTest, DisableExtension) {
  scoped_refptr<const extensions::Extension> test_extension =
      extensions::ExtensionBuilder("Test Extension")
          .SetID(kTestExtensionId)
          .Build();

  auto* registry = extensions::ExtensionRegistry::Get(profile());

  extensions::TestExtensionRegistryObserver load_observer(registry,
                                                          kTestExtensionId);
  extensions::ExtensionRegistrar::Get(profile())->AddExtension(
      test_extension.get());
  load_observer.WaitForExtensionLoaded();

  EXPECT_TRUE(registry->enabled_extensions().Contains(kTestExtensionId));
  EXPECT_FALSE(registry->disabled_extensions().Contains(kTestExtensionId));

  extensions::TestExtensionRegistryObserver unload_observer(registry,
                                                            kTestExtensionId);
  handler()->DisableExtension(nullptr, kTestExtensionId);
  unload_observer.WaitForExtensionUnloaded();

  EXPECT_FALSE(registry->enabled_extensions().Contains(kTestExtensionId));
  EXPECT_TRUE(registry->disabled_extensions().Contains(kTestExtensionId));
}

}  // namespace
