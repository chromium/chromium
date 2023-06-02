// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/extension_icon_loader.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"

class ExtensionIconLoaderTest : public BrowserWithTestWindowTest {
 public:
  ExtensionIconLoaderTest() = default;
  ExtensionIconLoaderTest(const ExtensionIconLoaderTest&) = delete;
  ExtensionIconLoaderTest& operator=(const ExtensionIconLoaderTest&) = delete;
  ~ExtensionIconLoaderTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile()));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
    extension_service_ =
        extensions::ExtensionSystem::Get(profile())->extension_service();
  }

  void OnIconFetched(base::RunLoop* run_loop, const gfx::ImageSkia& icon) {
    run_loop->Quit();
    loaded_icon_ = icon;
  }

  const gfx::ImageSkia& loaded_icon() { return loaded_icon_; }

 protected:
  raw_ptr<extensions::ExtensionService, DanglingUntriaged> extension_service_ =
      nullptr;
  gfx::ImageSkia loaded_icon_;
};

TEST_F(ExtensionIconLoaderTest, LoadExtensionWithIcon) {
  // Create and add an extension from a test manifest with an icon.
  base::FilePath test_file;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_file));
  test_file = test_file.AppendASCII("extensions/simple_with_icon");
  int error_code = 0;
  std::string error;
  JSONFileValueDeserializer deserializer(
      test_file.AppendASCII("manifest.json"));
  std::unique_ptr<base::Value> valid_value =
      deserializer.Deserialize(&error_code, &error);
  scoped_refptr<extensions::Extension> extension =
      extensions::Extension::Create(
          test_file, extensions::mojom::ManifestLocation::kUnpacked,
          valid_value->GetDict(), extensions::Extension::NO_FLAGS, &error);
  extension_service_->AddExtension(extension.get());

  extensions::ExtensionIconLoader loader;

  base::RunLoop run_loop;
  loader.Load(*extension, profile(),
              base::BindOnce(&ExtensionIconLoaderTest::OnIconFetched,
                             base::Unretained(this), &run_loop));
  run_loop.Run();

  // Check that the loaded icon is not a default icon.
  EXPECT_FALSE(loaded_icon().isNull());
  EXPECT_NE(loaded_icon().bitmap(),
            extensions::util::GetDefaultExtensionIcon().bitmap());
  EXPECT_NE(loaded_icon().bitmap(),
            extensions::util::GetDefaultAppIcon().bitmap());
}

TEST_F(ExtensionIconLoaderTest, LoadDefaultAppIcon) {
  // Create an app with no icon.
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(
          "extension", extensions::ExtensionBuilder::Type::PLATFORM_APP)
          .Build();
  extension_service_->AddExtension(extension.get());

  extensions::ExtensionIconLoader loader;

  base::RunLoop run_loop;
  loader.Load(*extension, profile(),
              base::BindOnce(&ExtensionIconLoaderTest::OnIconFetched,
                             base::Unretained(this), &run_loop));
  run_loop.Run();

  // Check that the default app icon is loaded.
  EXPECT_EQ(loaded_icon().bitmap(),
            extensions::util::GetDefaultAppIcon().bitmap());
}

TEST_F(ExtensionIconLoaderTest, LoadDefaultExtensionIcon) {
  // Create an extension with no icon.
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("extension").Build();
  extension_service_->AddExtension(extension.get());

  extensions::ExtensionIconLoader loader;

  base::RunLoop run_loop;
  loader.Load(*extension, profile(),
              base::BindOnce(&ExtensionIconLoaderTest::OnIconFetched,
                             base::Unretained(this), &run_loop));
  run_loop.Run();

  // Check that the default extension icon is loaded.
  EXPECT_EQ(loaded_icon().bitmap(),
            extensions::util::GetDefaultExtensionIcon().bitmap());
}
