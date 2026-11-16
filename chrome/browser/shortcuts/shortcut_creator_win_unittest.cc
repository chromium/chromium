// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/shortcut_creator.h"

#include <string>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/hash/hash.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_shortcut_win.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/browser/shortcuts/platform_util_win.h"
#include "chrome/browser/shortcuts/shortcut_creation_test_support.h"
#include "chrome/common/chrome_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_family.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace shortcuts {

namespace {

constexpr char kShortcutFileName[] = "Test Name.lnk";
constexpr char16_t kShortcutName[] = u"Test Name";
constexpr char16_t kShortcutWithInvalidCharsName[] = u"Test|Name";

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

base::FilePath GetUserDesktopPath() {
  return base::PathService::CheckedGet(base::DIR_USER_DESKTOP);
}

class ShortcutCreatorWinTest : public testing::Test {
 protected:
  const GURL kUrl = GURL("https://example.com/test?query=1s&basdf");
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    default_profile_path_ =
        temp_dir_.GetPath().AppendASCII(chrome::kInitialProfile);
    ASSERT_TRUE(base::CreateDirectory(default_profile_path_));
  }
  void VerifyShortcut(const base::FilePath& current_shortcut_path) const;
  void VerifyShortcut(const base::FilePath& current_shortcut_path,
                      const base::FilePath& profile_path,
                      const std::wstring& expected_arguments) const;
  void VerifyShortcut(const base::FilePath& current_shortcut_path,
                      const base::FilePath& profile_path,
                      const GURL& url,
                      const std::wstring& expected_arguments) const;
  const base::FilePath& default_profile_path() const {
    return default_profile_path_;
  }
  const base::FilePath& temp_dir_path() const { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
  base::FilePath default_profile_path_;
  base::ScopedPathOverride desktop_override_{base::DIR_USER_DESKTOP};
  base::win::ScopedCOMInitializer com_initializer_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

void ShortcutCreatorWinTest::VerifyShortcut(
    const base::FilePath& current_shortcut_path) const {
  VerifyShortcut(current_shortcut_path, default_profile_path(), kUrl,
                 L"--profile-directory=Default "
                 L"--ignore-profile-directory-if-not-exists -- "
                 L"https://example.com/test?query=1s&basdf");
}

void ShortcutCreatorWinTest::VerifyShortcut(
    const base::FilePath& current_shortcut_path,
    const base::FilePath& profile_path,
    const std::wstring& expected_arguments) const {
  VerifyShortcut(current_shortcut_path, profile_path, kUrl, expected_arguments);
}

void ShortcutCreatorWinTest::VerifyShortcut(
    const base::FilePath& current_shortcut_path,
    const base::FilePath& profile_path,
    const GURL& url,
    const std::wstring& expected_arguments) const {
  base::win::ShortcutProperties properties;
  properties.set_target(GetChromeProxyPath());
  std::string url_hash = base::NumberToString(base::PersistentHash(url.spec()));
  base::FilePath target_path = profile_path.Append(kWebShortcutsIconDirName)
                                   .AppendASCII(url_hash)
                                   .Append(L"shortcut.ico");
  properties.set_icon(target_path, 0);
  properties.set_arguments(expected_arguments);
  const base::FilePath expected_shortcut_path =
      GetUserDesktopPath().AppendASCII(kShortcutFileName);
  ASSERT_EQ(expected_shortcut_path, current_shortcut_path);
  ASSERT_TRUE(base::PathExists(current_shortcut_path));
  base::win::ValidateShortcut(current_shortcut_path, properties);
  // TODO(b/333024272): Verify images in .ico file are correct.

  // Verify that the shortcut matchers work correctly as well.
  EXPECT_THAT(expected_shortcut_path, IsShortcutForUrl(url));
  EXPECT_THAT(expected_shortcut_path, IsShortcutForProfile(profile_path));
  EXPECT_THAT(expected_shortcut_path, IsShortcutWithTitle(kShortcutName));
}

TEST_F(ShortcutCreatorWinTest, ShortcutCreated) {
  gfx::ImageFamily images =
      CreateImageFamily({{.size = 16, .color = SK_ColorCYAN},
                         {.size = 32, .color = SK_ColorBLUE},
                         {.size = 256, .color = SK_ColorGREEN}});

  ShortcutMetadata metadata(default_profile_path(), kUrl, kShortcutName,
                            std::move(images));
  base::test::TestFuture<const base::FilePath&, ShortcutCreatorResult> future;

  CreateShortcutOnUserDesktop(std::move(metadata), future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(ShortcutCreatorResult::kSuccess,
            future.Get<ShortcutCreatorResult>());

  VerifyShortcut(future.Get<base::FilePath>());
}

TEST_F(ShortcutCreatorWinTest, ShortcutWithInvalidCharsInNameCreated) {
  gfx::ImageFamily images =
      CreateImageFamily({{.size = 16, .color = SK_ColorCYAN},
                         {.size = 32, .color = SK_ColorBLUE},
                         {.size = 256, .color = SK_ColorGREEN}});

  // The '|' in kShortcutWithInvalidCharsName should be replaced with ' ' and
  // match kShortcutFileName.
  ShortcutMetadata metadata(default_profile_path(), kUrl,
                            kShortcutWithInvalidCharsName, std::move(images));
  base::test::TestFuture<const base::FilePath&, ShortcutCreatorResult> future;

  CreateShortcutOnUserDesktop(std::move(metadata), future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(ShortcutCreatorResult::kSuccess,
            future.Get<ShortcutCreatorResult>());

  VerifyShortcut(future.Get<base::FilePath>());
}

TEST_F(ShortcutCreatorWinTest, ShortcutWithSpacesInProfilePathCreated) {
  base::FilePath profile_path = temp_dir_path().AppendASCII(
      base::StrCat({chrome::kMultiProfileDirPrefix, "1"}));
  ASSERT_TRUE(base::CreateDirectory(profile_path));

  gfx::ImageFamily images =
      CreateImageFamily({{.size = 16, .color = SK_ColorCYAN},
                         {.size = 32, .color = SK_ColorBLUE},
                         {.size = 256, .color = SK_ColorGREEN}});

  ShortcutMetadata metadata(profile_path, kUrl, kShortcutName,
                            std::move(images));
  base::test::TestFuture<const base::FilePath&, ShortcutCreatorResult> future;

  CreateShortcutOnUserDesktop(std::move(metadata), future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(ShortcutCreatorResult::kSuccess,
            future.Get<ShortcutCreatorResult>());

  VerifyShortcut(future.Get<base::FilePath>(), profile_path,
                 L"--profile-directory=\"Profile 1\" "
                 L"--ignore-profile-directory-if-not-exists -- "
                 L"https://example.com/test?query=1s&basdf");
}

TEST_F(ShortcutCreatorWinTest, ShortcutWithBackslashesInUrlCreated) {
  const GURL url_with_backslashes("https://example.com/test#a\\b\\");
  ASSERT_EQ(url_with_backslashes.spec(), "https://example.com/test#a\\b\\");

  gfx::ImageFamily images =
      CreateImageFamily({{.size = 16, .color = SK_ColorCYAN},
                         {.size = 32, .color = SK_ColorBLUE},
                         {.size = 256, .color = SK_ColorGREEN}});

  ShortcutMetadata metadata(default_profile_path(), url_with_backslashes,
                            kShortcutName, std::move(images));
  base::test::TestFuture<const base::FilePath&, ShortcutCreatorResult> future;

  CreateShortcutOnUserDesktop(std::move(metadata), future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(ShortcutCreatorResult::kSuccess,
            future.Get<ShortcutCreatorResult>());

  VerifyShortcut(future.Get<base::FilePath>(), default_profile_path(),
                 url_with_backslashes,
                 L"--profile-directory=Default "
                 L"--ignore-profile-directory-if-not-exists -- "
                 L"\"https://example.com/test#a\\b\\\\\"");
}

TEST_F(ShortcutCreatorWinTest, ShortcutWithSpacesQuotesAndSwitchInUrlCreated) {
  const GURL url_with_switch("data:text/html,foo\" --evil-switch");
  ASSERT_EQ(url_with_switch.spec(), "data:text/html,foo\" --evil-switch");

  gfx::ImageFamily images =
      CreateImageFamily({{.size = 16, .color = SK_ColorCYAN},
                         {.size = 32, .color = SK_ColorBLUE},
                         {.size = 256, .color = SK_ColorGREEN}});

  ShortcutMetadata metadata(default_profile_path(), url_with_switch,
                            kShortcutName, std::move(images));
  base::test::TestFuture<const base::FilePath&, ShortcutCreatorResult> future;

  CreateShortcutOnUserDesktop(std::move(metadata), future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(ShortcutCreatorResult::kSuccess,
            future.Get<ShortcutCreatorResult>());

  VerifyShortcut(future.Get<base::FilePath>(), default_profile_path(),
                 url_with_switch,
                 L"--profile-directory=Default "
                 L"--ignore-profile-directory-if-not-exists -- "
                 L"\"data:text/html,foo\\\" --evil-switch\"");
}

}  // namespace

}  // namespace shortcuts
