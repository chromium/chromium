// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/icon_badging.h"

#include <iterator>
#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/numerics/clamped_math.h"
#include "base/path_service.h"
#include "base/strings/to_string.h"
#include "base/test/gmock_expected_support.h"
#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "chrome/browser/shortcuts/image_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_family.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/test/sk_gmock_support.h"

namespace shortcuts {

namespace {

constexpr SkColor kSiteColor = SK_ColorRED;

gfx::Image CreateImage(int icon_size, SkColor color) {
  return gfx::test::CreateImage(icon_size, color);
}

bool ShouldRebaselineTestImages() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return command_line.HasSwitch("rebaseline-shortcuts-icon-testing");
}

base::FilePath GetCompileTimeTestFolders() {
  base::FilePath compile_time_folder;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  compile_time_folder = base::FilePath(FILE_PATH_LITERAL("chrome_branded"));
#elif BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
  compile_time_folder =
      base::FilePath(FILE_PATH_LITERAL("chrome_for_testing_branded"));
#else
  compile_time_folder = base::FilePath(FILE_PATH_LITERAL("chromium"));
#endif
  return compile_time_folder;
}

base::expected<base::FilePath, std::string> GetPathForShortcutBadgedIcons() {
  base::FilePath chrome_src_dir;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &chrome_src_dir)) {
    return base::unexpected("Could not find src directory.");
  }

  base::FilePath folder_path =
      chrome_src_dir
          .Append(FILE_PATH_LITERAL("chrome/test/data/shortcuts/badging_icons"))
          .Append(GetCompileTimeTestFolders());
  if (!base::PathExists(folder_path)) {
    base::CreateDirectory(folder_path);
  }

  return folder_path;
}

void WriteTestIconsToDiskOrDie(const gfx::ImageFamily& family) {
  auto shortcuts_test_path = GetPathForShortcutBadgedIcons();
  CHECK(shortcuts_test_path.has_value())
      << " Unable to parse icons folder for testing";
  base::FilePath shortcuts_path(shortcuts_test_path.value());

  for (const gfx::Image& image : family) {
    std::vector<uint8_t> png_output;
    const SkBitmap image_bitmap = image.AsBitmap();
    const bool png_encode_status = gfx::PNGCodec::Encode(
        reinterpret_cast<const unsigned char*>(image_bitmap.getPixels()),
        gfx::PNGCodec::FORMAT_SkBitmap, image.Size(),
        static_cast<int>(image_bitmap.rowBytes()),
        /*discard_transparency=*/true, std::vector<gfx::PNGCodec::Comment>(),
        &png_output);
    CHECK(png_encode_status);

    std::string width = base::ToString(image.Width());
    base::FilePath out_filename;
#if BUILDFLAG(IS_WIN)
    std::wstring width_str(width.begin(), width.end());
    out_filename = shortcuts_path.Append(width_str);
#else
    out_filename = shortcuts_path.Append(width);
#endif  // BUILDFLAG(IS_WIN)

    out_filename = out_filename.AddExtension(FILE_PATH_LITERAL(".png"));
    const bool success = base::WriteFile(out_filename, png_output);
    CHECK(success) << base::StrCat({"Failed to write icon of size ", width});
  }
}

// This is hardcoded based on the sizes of kSizesNeededForShortcutCreation.
int GetOsSpecificSizes() {
#if BUILDFLAG(IS_MAC)
  return 5;
#elif BUILDFLAG(IS_LINUX)
  return 2;
#elif BUILDFLAG(IS_WIN)
  return 4;
#endif
}

}  // namespace

// Verifies that badging logic works as intended by comparing with icons stored
// on disk. The icons are stored in file names corresponding to their sizes, and
// are stored inside `chrome/test/data/shortcuts/badging_icons/chromium` for non
// branded builds and `chrome/test/data/shortcuts/badging_icons/chrome_branded`
// for branded builds.
// If badging behavior changes, the icons in the listed folders would need to be
// updated. That can be done manually, or can be done automatically by using the
// `rebaseline-shortcuts-icon-testing` command line flag. Example usage:
// out/Default/unit_tests --gtest_filter=*IconBadgingTest*
// --rebaseline-shortcuts-icon-testing

TEST(IconBadgingTest, VerifyFromDisk) {
  std::vector<SkBitmap> bitmaps;

  // The sizes have been chosen randomly for testing values in the extremities.
  bitmaps.emplace_back(CreateImage(20, kSiteColor).AsBitmap());
  bitmaps.emplace_back(CreateImage(1000, kSiteColor).AsBitmap());
  bitmaps.emplace_back(CreateImage(150, kSiteColor).AsBitmap());

  gfx::ImageFamily family = shortcuts::ApplyProductLogoBadgeToIcons(bitmaps);
  if (ShouldRebaselineTestImages()) {
    WriteTestIconsToDiskOrDie(family);
  }

  EXPECT_EQ(std::distance(family.begin(), family.end()), GetOsSpecificSizes());

  for (const gfx::Image& image_icon : family) {
    std::string width = base::ToString(image_icon.Width());

    SkBitmap expected_bitmap;
    base::FilePath icon_path_relative =
        base::FilePath(FILE_PATH_LITERAL("shortcuts/badging_icons"))
            .Append(GetCompileTimeTestFolders());
#if BUILDFLAG(IS_WIN)
    std::wstring width_str(width.begin(), width.end());
    icon_path_relative = icon_path_relative.Append(width_str);
#else
    icon_path_relative = icon_path_relative.Append(width);
#endif  // BUILDFLAG(IS_WIN)

    icon_path_relative =
        icon_path_relative.AddExtension(FILE_PATH_LITERAL(".png"));

    ASSERT_OK_AND_ASSIGN(expected_bitmap,
                         LoadImageFromTestFile(icon_path_relative));
    EXPECT_THAT(expected_bitmap,
                gfx::test::EqualsBitmap(image_icon.AsBitmap()));
  }
}

}  // namespace shortcuts
