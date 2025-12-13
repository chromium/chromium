// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/extension_icon_loader.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"

class ExtensionIconLoaderTest : public extensions::ExtensionBrowserTest {
 public:
  ExtensionIconLoaderTest() = default;
  ExtensionIconLoaderTest(const ExtensionIconLoaderTest&) = delete;
  ExtensionIconLoaderTest& operator=(const ExtensionIconLoaderTest&) = delete;
  ~ExtensionIconLoaderTest() override = default;

  void OnIconFetched(base::RunLoop* run_loop, const gfx::ImageSkia& icon) {
    run_loop->Quit();
    loaded_icon_ = icon;
  }

  const gfx::ImageSkia& loaded_icon() { return loaded_icon_; }

 protected:
  gfx::ImageSkia loaded_icon_;
};

IN_PROC_BROWSER_TEST_F(ExtensionIconLoaderTest, LoadExtensionWithIcon) {
  // Create and add an extension from a test manifest with an icon.
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_icon"));
  ASSERT_TRUE(extension);

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

IN_PROC_BROWSER_TEST_F(ExtensionIconLoaderTest, LoadDefaultAppIcon) {
  // Create an app with no icon.
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(
          "extension", extensions::ExtensionBuilder::Type::PLATFORM_APP)
          .Build();
  extension_registrar()->AddExtension(extension);

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

IN_PROC_BROWSER_TEST_F(ExtensionIconLoaderTest, LoadDefaultExtensionIcon) {
  // Create an extension with no icon.
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("extension").Build();
  extension_registrar()->AddExtension(extension);

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
