// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/shortcut_creator.h"

#import <AppKit/AppKit.h>

#include <iomanip>
#include <string>

#include "base/apple/foundation_util.h"
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/shortcuts/chrome_webloc_file.h"
#include "skia/ext/skia_utils_mac.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_family.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace shortcuts {

namespace {

base::FilePath GetUserDesktopPath() {
  return base::PathService::CheckedGet(base::DIR_USER_DESKTOP);
}

struct ImageDesc {
  int size;
  SkColor color;
};

gfx::ImageFamily CreateImageFamily(std::vector<ImageDesc> images) {
  gfx::ImageFamily image_family;
  for (const ImageDesc& image_desc : images) {
    image_family.Add(gfx::test::CreateImage(image_desc.size, image_desc.color));
  }
  return image_family;
}

void CheckColorForImageRep(NSImageRep* rep, SkColor expected_color) {
  SkBitmap bitmap(
      skia::NSImageRepToSkBitmap(rep, rep.size, /*is_opaque=*/false));

  // Check the corners and center pixel, which should be good enough for these
  // tests.
  gfx::test::CheckColors(expected_color, bitmap.getColor(0, 0));
  gfx::test::CheckColors(expected_color,
                         bitmap.getColor(bitmap.width() - 1, 0));
  gfx::test::CheckColors(expected_color,
                         bitmap.getColor(0, bitmap.height() - 1));
  gfx::test::CheckColors(
      expected_color, bitmap.getColor(bitmap.width() - 1, bitmap.height() - 1));
  gfx::test::CheckColors(
      expected_color, bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2));
}

}  // namespace

class ShortcutCreatorMacTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(profile_path_.CreateUniqueTempDir()); }
  void TearDown() override { EXPECT_TRUE(profile_path_.Delete()); }

  const base::FilePath& profile_path() { return profile_path_.GetPath(); }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedPathOverride desktop_override_{base::DIR_USER_DESKTOP};
  base::ScopedTempDir profile_path_;
};

TEST_F(ShortcutCreatorMacTest, ShortcutCreated) {
  gfx::ImageFamily images =
      CreateImageFamily({{.size = 128, .color = SK_ColorMAGENTA}});

  base::test::TestFuture<const base::FilePath&, ShortcutCreatorResult> future;

  ShortcutMetadata metadata(profile_path(), GURL("https://example.com/test"),
                            u"Test Name", std::move(images));

  CreateShortcutOnUserDesktop(std::move(metadata), future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(ShortcutCreatorResult::kSuccess,
            future.Get<ShortcutCreatorResult>());

  const base::FilePath& current_shortcut_path = future.Get<base::FilePath>();

  const base::FilePath expected_path =
      GetUserDesktopPath().AppendASCII("Test Name.crwebloc");
  ASSERT_EQ(current_shortcut_path, expected_path);
  ASSERT_TRUE(base::PathExists(expected_path));

  std::optional<ChromeWeblocFile> file =
      ChromeWeblocFile::LoadFromFile(expected_path);
  EXPECT_TRUE(file.has_value());

  EXPECT_EQ(GURL("https://example.com/test"), file->target_url());
  EXPECT_EQ(profile_path().BaseName(), file->profile_path_name().path());

  // This doesn't verify that the created shortcut is associated with the
  // correct chrome version, as such a check wouldn't make sense when it is
  // the unit test rather than chrome that created the shortcut.
}

TEST_F(ShortcutCreatorMacTest, ShortcutCreatedWithCorrectIcons) {
  std::vector<ImageDesc> image_descs = {{.size = 16, .color = SK_ColorCYAN},
                                        {.size = 32, .color = SK_ColorBLUE},
                                        {.size = 64, .color = SK_ColorRED},
                                        {.size = 128, .color = SK_ColorMAGENTA},
                                        {.size = 256, .color = SK_ColorGREEN}};
  gfx::ImageFamily images = CreateImageFamily(image_descs);

  base::test::TestFuture<const base::FilePath&, ShortcutCreatorResult> future;
  ShortcutMetadata metadata(profile_path(), GURL("https://example.com/test"),
                            u"Test Name", std::move(images));

  CreateShortcutOnUserDesktop(std::move(metadata), future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(ShortcutCreatorResult::kSuccess,
            future.Get<ShortcutCreatorResult>());
  const base::FilePath& current_shortcut_path = future.Get<base::FilePath>();

  const base::FilePath expected_path =
      GetUserDesktopPath().AppendASCII("Test Name.crwebloc");
  ASSERT_EQ(current_shortcut_path, expected_path);
  ASSERT_TRUE(base::PathExists(expected_path));

  NSImage* icon = [NSWorkspace.sharedWorkspace
      iconForFile:base::apple::FilePathToNSString(expected_path)];
  ASSERT_TRUE(icon);

  for (NSImageRep* rep in icon.representations) {
    SkBitmap bitmap(
        skia::NSImageRepToSkBitmap(rep, rep.size, /*is_opaque=*/true));
    EXPECT_EQ(rep.pixelsHigh, rep.pixelsWide);
    SkColor expected_color = 0;
    if (rep.pixelsHigh == rep.size.height) {
      // For whatever reason, 1x icons seem to be resized from the largest size
      // when retrieved via NSWorkspace iconForFile:.
      expected_color = SK_ColorGREEN;
      // 1024 sized icons aren't correctly generated at all in 1x size, so skip
      // those.
      if (rep.pixelsHigh == 1024) {
        continue;
      }
    } else {
      for (const auto& desc : image_descs) {
        expected_color = desc.color;
        if (desc.size >= rep.pixelsHigh) {
          break;
        }
      }
    }
    CheckColorForImageRep(rep, expected_color);
  }
}

}  // namespace shortcuts
