// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_action_icon_factory.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/image_util.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "skia/ext/image_operations.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/skia_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

bool ImageRepsAreEqual(const gfx::ImageSkiaRep& image_rep1,
                       const gfx::ImageSkiaRep& image_rep2) {
  return image_rep1.scale() == image_rep2.scale() &&
         gfx::BitmapsAreEqual(image_rep1.GetBitmap(), image_rep2.GetBitmap());
}

gfx::Image EnsureImageSize(const gfx::Image& original, int size) {
  const SkBitmap* original_bitmap = original.ToSkBitmap();
  if (original_bitmap->width() == size && original_bitmap->height() == size)
    return original;

  SkBitmap resized = skia::ImageOperations::Resize(
      *original.ToSkBitmap(), skia::ImageOperations::RESIZE_LANCZOS3,
      size, size);
  return gfx::Image::CreateFrom1xBitmap(resized);
}

gfx::ImageSkiaRep CreateBlankRep(int size_dip, float scale) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(static_cast<int>(size_dip * scale),
                        static_cast<int>(size_dip * scale));
  bitmap.eraseColor(SkColorSetARGB(0, 0, 0, 0));
  return gfx::ImageSkiaRep(bitmap, scale);
}

gfx::Image LoadIcon(const std::string& filename) {
  base::FilePath path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("extensions/api_test").AppendASCII(filename);

  std::string file_contents;
  base::ReadFileToString(path, &file_contents);
  const unsigned char* data =
      reinterpret_cast<const unsigned char*>(file_contents.data());

  SkBitmap bitmap;
  gfx::PNGCodec::Decode(data, file_contents.length(), &bitmap);

  return gfx::Image::CreateFrom1xBitmap(bitmap);
}

class ExtensionActionIconFactoryTest
    : public testing::Test,
      public ExtensionActionIconFactory::Observer {
 public:
  ExtensionActionIconFactoryTest() : quit_in_icon_updated_(false) {}

  ExtensionActionIconFactoryTest(const ExtensionActionIconFactoryTest&) =
      delete;
  ExtensionActionIconFactoryTest& operator=(
      const ExtensionActionIconFactoryTest&) = delete;

  ~ExtensionActionIconFactoryTest() override {}

  void WaitForIconUpdate() {
    quit_in_icon_updated_ = true;
    loop_.Run();
    quit_in_icon_updated_ = false;
  }

  scoped_refptr<Extension> CreateExtension(const char* name,
                                           ManifestLocation location) {
    // Create and load an extension.
    base::FilePath test_file;
    if (!base::PathService::Get(chrome::DIR_TEST_DATA, &test_file)) {
      EXPECT_FALSE(true);
      return nullptr;
    }
    test_file = test_file.AppendASCII("extensions/api_test").AppendASCII(name);
    int error_code = 0;
    std::string error;
    JSONFileValueDeserializer deserializer(
        test_file.AppendASCII("manifest.json"));
    std::unique_ptr<base::Value> valid_value =
        deserializer.Deserialize(&error_code, &error);
    EXPECT_EQ(0, error_code) << error;
    if (error_code != 0)
      return nullptr;

    EXPECT_TRUE(valid_value.get() && valid_value->is_dict());
    if (!valid_value || !valid_value->is_dict())
      return nullptr;

    scoped_refptr<Extension> extension =
        Extension::Create(test_file, location, valid_value->GetDict(),
                          Extension::NO_FLAGS, &error);
    EXPECT_TRUE(extension.get()) << error;
    if (extension.get())
      extension_service_->AddExtension(extension.get());
    return extension;
  }

  // testing::Test overrides:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    extension_service_ = static_cast<extensions::TestExtensionSystem*>(
        extensions::ExtensionSystem::Get(profile_.get()))->
        CreateExtensionService(&command_line, base::FilePath(), false);
  }

  void TearDown() override {
    profile_.reset();  // Get all DeleteSoon calls sent to ui_loop_.
  }

  // ExtensionActionIconFactory::Observer overrides:
  void OnIconUpdated() override {
    if (quit_in_icon_updated_)
      loop_.QuitWhenIdle();
  }

  gfx::ImageSkia GetFavicon() {
    return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_EXTENSIONS_FAVICON);
  }

  ExtensionAction* GetExtensionAction(const Extension& extension) {
    return ExtensionActionManager::Get(profile_.get())
        ->GetExtensionAction(extension);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  bool quit_in_icon_updated_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<ExtensionService, DanglingUntriaged> extension_service_;
  base::RunLoop loop_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  user_manager::ScopedUserManager user_manager_{
      std::make_unique<user_manager::UserManagerImpl>(
          std::make_unique<ash::UserManagerDelegateImpl>(),
          g_browser_process->local_state(),
          ash::CrosSettings::Get())};
#endif
};

// If there is no default icon, and the icon has not been set using |SetIcon|,
// the factory should return the placeholder icon.
TEST_F(ExtensionActionIconFactoryTest, NoIcons) {
  // Load an extension that has browser action without default icon set in the
  // manifest and does not call |SetIcon| by default.
  scoped_refptr<Extension> extension(
      CreateExtension("browser_action/no_icon", ManifestLocation::kUnpacked));
  ASSERT_TRUE(extension.get() != nullptr);
  ExtensionAction* action = GetExtensionAction(*extension);
  ASSERT_TRUE(action);
  ASSERT_FALSE(action->default_icon());
  ASSERT_TRUE(action->GetExplicitlySetIcon(0 /*tab id*/).IsEmpty());

  ExtensionActionIconFactory icon_factory(extension.get(), action, this);

  gfx::Image icon = icon_factory.GetIcon(0);

  EXPECT_TRUE(ImageRepsAreEqual(
      action->GetDefaultIconImage().ToImageSkia()->GetRepresentation(1.0f),
      icon.ToImageSkia()->GetRepresentation(1.0f)));
}

// If the explicitly-set icon is invisible, |ExtensionAction::GetIcon| should
// return the placeholder icon.
TEST_F(ExtensionActionIconFactoryTest, InvisibleIcon) {
  // Load an extension that has browser action with a default icon set in the
  // manifest, but that icon is not sufficiently visible.
  scoped_refptr<Extension> extension(CreateExtension(
      "browser_action/invisible_icon", ManifestLocation::kInternal));

  // Check that the default icon is not sufficiently visible.
  ASSERT_TRUE(extension);
  ExtensionAction* action = GetExtensionAction(*extension);
  ASSERT_TRUE(action);
  EXPECT_TRUE(action->default_icon());
  gfx::Image default_icon = action->GetDefaultIconImage();
  EXPECT_FALSE(default_icon.IsEmpty());
  const SkBitmap* const bitmap = default_icon.ToSkBitmap();
  ASSERT_TRUE(bitmap);
  EXPECT_FALSE(extensions::image_util::IsIconSufficientlyVisible(*bitmap));

  // Set the flag for testing.
  ExtensionActionIconFactory::SetAllowInvisibleIconsForTest(false);

  ExtensionActionIconFactory icon_factory(extension.get(), action, this);

  gfx::Image icon = icon_factory.GetIcon(0);
  // The default icon should not be returned, since it's invisible.
  // The placeholder icon should be returned instead.
  EXPECT_TRUE(ImageRepsAreEqual(
      action->GetPlaceholderIconImage().ToImageSkia()->GetRepresentation(1.0f),
      icon.ToImageSkia()->GetRepresentation(1.0f)));

  // Reset the flag for testing.
  ExtensionActionIconFactory::SetAllowInvisibleIconsForTest(true);
}

// If the icon has been set using |SetIcon|, the factory should return that
// icon.
TEST_F(ExtensionActionIconFactoryTest, AfterSetIcon) {
  // Load an extension that has browser action without default icon set in the
  // manifest and does not call |SetIcon| by default (but has an browser action
  // icon resource).
  scoped_refptr<Extension> extension(
      CreateExtension("browser_action/no_icon", ManifestLocation::kUnpacked));
  ASSERT_TRUE(extension.get() != nullptr);
  ExtensionAction* action = GetExtensionAction(*extension);
  ASSERT_TRUE(action);
  ASSERT_FALSE(action->default_icon());
  ASSERT_TRUE(action->GetExplicitlySetIcon(0 /*tab id*/).IsEmpty());

  gfx::Image set_icon = LoadIcon("browser_action/no_icon/icon.png");
  ASSERT_FALSE(set_icon.IsEmpty());

  action->SetIcon(0, set_icon);

  ASSERT_FALSE(action->GetExplicitlySetIcon(0 /*tab id*/).IsEmpty());

  ExtensionActionIconFactory icon_factory(extension.get(), action, this);

  gfx::Image icon = icon_factory.GetIcon(0);

  EXPECT_TRUE(ImageRepsAreEqual(
      set_icon.ToImageSkia()->GetRepresentation(1.0f),
      icon.ToImageSkia()->GetRepresentation(1.0f)));

  // It should still return the default icon for another tab.
  icon = icon_factory.GetIcon(1);

  EXPECT_TRUE(ImageRepsAreEqual(
      action->GetDefaultIconImage().ToImageSkia()->GetRepresentation(1.0f),
      icon.ToImageSkia()->GetRepresentation(1.0f)));
}

// If there is a default icon, and the icon has not been set using |SetIcon|,
// the factory should return the default icon.
TEST_F(ExtensionActionIconFactoryTest, DefaultIcon) {
  // Load an extension that has browser action without default icon set in the
  // manifest and does not call |SetIcon| by default (but has an browser action
  // icon resource).
  scoped_refptr<Extension> extension(
      CreateExtension("browser_action/no_icon", ManifestLocation::kUnpacked));
  ASSERT_TRUE(extension.get() != nullptr);
  ExtensionAction* action = GetExtensionAction(*extension);
  ASSERT_TRUE(action);
  ASSERT_FALSE(action->default_icon());
  ASSERT_TRUE(action->GetExplicitlySetIcon(0 /*tab id*/).IsEmpty());

  scoped_refptr<const Extension> extension_with_icon =
      CreateExtension("browser_action_with_icon", ManifestLocation::kUnpacked);
  ASSERT_TRUE(extension_with_icon);

  int icon_size = ExtensionAction::ActionIconSize();
  gfx::Image default_icon =
      EnsureImageSize(LoadIcon("browser_action_with_icon/icon.png"), icon_size);
  ASSERT_FALSE(default_icon.IsEmpty());

  action = GetExtensionAction(*extension_with_icon);
  ASSERT_TRUE(action->default_icon());

  ExtensionActionIconFactory icon_factory(extension_with_icon.get(), action,
                                          this);

  gfx::Image icon = icon_factory.GetIcon(0);

  // The icon should be loaded asynchronously. Initially a transparent icon
  // should be returned.
  EXPECT_TRUE(ImageRepsAreEqual(
      CreateBlankRep(icon_size, 1.0f),
      icon.ToImageSkia()->GetRepresentation(1.0f)));

  WaitForIconUpdate();

  icon = icon_factory.GetIcon(0);

  // The default icon representation should be loaded at this point.
  EXPECT_TRUE(ImageRepsAreEqual(
      default_icon.ToImageSkia()->GetRepresentation(1.0f),
      icon.ToImageSkia()->GetRepresentation(1.0f)));

  // The same icon should be returned for the other tabs.
  icon = icon_factory.GetIcon(1);

  EXPECT_TRUE(ImageRepsAreEqual(
      default_icon.ToImageSkia()->GetRepresentation(1.0f),
      icon.ToImageSkia()->GetRepresentation(1.0f)));

}

}  // namespace
}  // namespace extensions
