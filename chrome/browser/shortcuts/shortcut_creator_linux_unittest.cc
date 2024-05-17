// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/shortcut_creator_linux.h"

#include <algorithm>
#include <functional>

#include "base/base_paths.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/hash/md5.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/gmock_expected_support.h"
#include "base/types/expected.h"
#include "chrome/browser/shortcuts/fake_linux_xdg_wrapper.h"
#include "chrome/browser/shortcuts/linux_xdg_wrapper.h"
#include "chrome/browser/shortcuts/shortcut_creation_test_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_family.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/test/sk_gmock_support.h"
#include "url/gurl.h"

namespace shortcuts {
namespace {
using base::test::ValueIs;
using gfx::test::CreateBitmap;
using gfx::test::EqualsBitmap;
using testing::Field;
using testing::HasSubstr;
using testing::StartsWith;

std::string CollapseWhitespace(const std::string& input) {
  std::string trimmed;
  base::TrimWhitespaceASCII(input, base::TrimPositions::TRIM_ALL, &trimmed);
  return base::CollapseWhitespaceASCII(trimmed, false);
}

base::FilePath GetUserDesktopPath() {
  return base::PathService::CheckedGet(base::DIR_USER_DESKTOP);
}

base::expected<SkBitmap, std::string> LoadIcon(
    const base::FilePath& icon_path) {
  if (!base::PathExists(icon_path)) {
    return base::unexpected("Icon path does not exist.");
  }
  std::string icon_data;
  if (!base::ReadFileToString(icon_path, &icon_data)) {
    return base::unexpected("Could not read icon file.");
  }
  base::span<const uint8_t> bytes = base::as_byte_span(icon_data);
  SkBitmap icon;
  if (!gfx::PNGCodec::Decode(bytes.data(), bytes.size(), &icon)) {
    return base::unexpected("Could not decode icon file.");
  }
  return base::ok(icon);
}

class ShortcutCreatorLinuxTest : public testing::Test {
 protected:
  const GURL kUrl = GURL("https://example.com/test?query=1s&basdf");
  // The hash is the result of applying MD5 to the kUrl above.
  const std::string kShortcutBaseName =
      "shortcut-c0439743e18462397982265de7846820.png";

  base::FilePath GetShortcutIconDir() {
    return profile_path_.GetPath().AppendASCII("Web Shortcut Icons");
  }

  void SetUp() override { ASSERT_TRUE(profile_path_.CreateUniqueTempDir()); }

  const base::FilePath& profile_path() const { return profile_path_.GetPath(); }

 private:
  ShortcutCreationTestSupport test_support_;
  base::ScopedTempDir profile_path_;
};

TEST_F(ShortcutCreatorLinuxTest, ShortcutCreatedWithIcons) {
  gfx::Image image = gfx::test::CreateImage(/*size=*/16, SK_ColorCYAN);

  base::FilePath shortcut_icon_path =
      GetShortcutIconDir().Append(kShortcutBaseName);

  EXPECT_FALSE(base::PathExists(shortcut_icon_path));
  EXPECT_FALSE(base::DirectoryExists(GetShortcutIconDir()));

  FakeLinuxXdgWrapper xdg_wrapper;
  ShortcutCreatorOutput creation_metadata = CreateShortcutOnLinuxDesktop(
      "Test Name", kUrl, std::move(image), profile_path(), xdg_wrapper);
  EXPECT_EQ(ShortcutCreatorResult::kSuccess, creation_metadata.result);
  EXPECT_TRUE(base::PathExists(creation_metadata.shortcut_path));

  EXPECT_TRUE(base::PathExists(shortcut_icon_path));
  EXPECT_THAT(LoadIcon(shortcut_icon_path),
              ValueIs(EqualsBitmap(CreateBitmap(16, SK_ColorCYAN))));
}

TEST_F(ShortcutCreatorLinuxTest, ShortcutCreatedWithCorrectFile) {
  gfx::Image image = gfx::test::CreateImage(/*size=*/16, SK_ColorCYAN);
  base::FilePath shortcut_icon_path =
      GetShortcutIconDir().Append(kShortcutBaseName);

  FakeLinuxXdgWrapper xdg_wrapper;
  ShortcutCreatorOutput creation_metadata = CreateShortcutOnLinuxDesktop(
      "Test Name", kUrl, std::move(image), profile_path(), xdg_wrapper);
  EXPECT_EQ(ShortcutCreatorResult::kSuccess, creation_metadata.result);

  const base::FilePath& shortcut_path = creation_metadata.shortcut_path;
  EXPECT_TRUE(base::PathExists(shortcut_path));

  ASSERT_EQ(xdg_wrapper.GetInstalls().size(), 1u);
  base::FilePath desktop_file = xdg_wrapper.GetInstalls()[0];
  EXPECT_EQ(GetUserDesktopPath().AppendASCII("chrome-Test_Name.desktop"),
            desktop_file);
  EXPECT_EQ(shortcut_path, desktop_file);
  std::string file;
  ASSERT_TRUE(base::ReadFileToString(desktop_file, &file));

  std::string expected_file_prefix = R"(
      #!/usr/bin/env xdg-open
      [Desktop Entry]
      Version=1.0
      Type=Application
      Name=Test Name
    )";

  EXPECT_THAT(CollapseWhitespace(file),
              StartsWith(CollapseWhitespace(expected_file_prefix)));

  // Exec
  // Note: The profile directory is expected to be simply the base name, and not
  // the full path.
  std::string expected_command_line_args = base::StringPrintf(
      "--profile-directory=%s --ignore-profile-directory-if-not-exists \"%s\"",
      profile_path().BaseName().value().c_str(), kUrl.spec().c_str());
  EXPECT_THAT(file, HasSubstr(expected_command_line_args));

  // Icon
  EXPECT_THAT(file,
              HasSubstr(base::StrCat({"Icon=", shortcut_icon_path.value()})));

  // URL
  EXPECT_THAT(file, HasSubstr(base::StrCat({"URL=", kUrl.spec()})));

  // Verify that the shortcut matchers work correctly as well.
  EXPECT_THAT(desktop_file, IsShortcutForUrl(kUrl));
  EXPECT_THAT(desktop_file, IsShortcutForProfile(profile_path()));
  EXPECT_THAT(desktop_file, IsShortcutWithTitle(u"Test Name"));
}

}  // namespace

}  // namespace shortcuts
