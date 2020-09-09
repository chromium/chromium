// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/shortcut_info.h"

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "url/gurl.h"

blink::Manifest::ImageResource CreateImage(const std::string& url,
                                           const gfx::Size& size) {
  blink::Manifest::ImageResource image;
  image.src = GURL("https://example.com" + url);
  image.sizes.push_back(size);
  image.purpose.push_back(blink::Manifest::ImageResource::Purpose::ANY);
  return image;
}

blink::Manifest::ShortcutItem CreateShortcut(
    const std::string& name,
    const std::vector<blink::Manifest::ImageResource>& icons) {
  blink::Manifest::ShortcutItem shortcut;
  shortcut.name = base::UTF8ToUTF16(name);
  shortcut.url = GURL("https://example.com/");
  shortcut.icons = icons;
  return shortcut;
}

class ShortcutInfoTest : public testing::Test {
 public:
  ShortcutInfoTest() : info_(GURL()) {}

 protected:
  ShortcutInfo info_;
  blink::Manifest manifest_;

  DISALLOW_COPY_AND_ASSIGN(ShortcutInfoTest);
};

TEST_F(ShortcutInfoTest, AllAttributesUpdate) {
  info_.name = base::ASCIIToUTF16("old name");
  manifest_.name = base::ASCIIToUTF16("new name");

  info_.short_name = base::ASCIIToUTF16("old short name");
  manifest_.short_name = base::ASCIIToUTF16("new short name");

  info_.url = GURL("https://old.com/start");
  manifest_.start_url = GURL("https://new.com/start");

  info_.scope = GURL("https://old.com/");
  manifest_.scope = GURL("https://new.com/");

  info_.display = blink::mojom::DisplayMode::kStandalone;
  manifest_.display = blink::mojom::DisplayMode::kFullscreen;

  info_.theme_color = 0xffff0000;
  manifest_.theme_color = 0xffcc0000;

  info_.background_color = 0xffaa0000;
  manifest_.background_color = 0xffbb0000;

  info_.icon_urls.push_back("https://old.com/icon.png");
  blink::Manifest::ImageResource icon;
  icon.src = GURL("https://new.com/icon.png");
  manifest_.icons.push_back(icon);

  info_.UpdateFromManifest(manifest_);

  ASSERT_EQ(manifest_.name, info_.name);
  ASSERT_EQ(manifest_.short_name, info_.short_name);
  ASSERT_EQ(manifest_.start_url, info_.url);
  ASSERT_EQ(manifest_.scope, info_.scope);
  ASSERT_EQ(manifest_.display, info_.display);
  ASSERT_EQ(manifest_.theme_color, info_.theme_color);
  ASSERT_EQ(manifest_.background_color, info_.background_color);
  ASSERT_EQ(1u, info_.icon_urls.size());
  ASSERT_EQ(manifest_.icons[0].src, GURL(info_.icon_urls[0]));
}

TEST_F(ShortcutInfoTest, NameFallsBackToShortName) {
  manifest_.short_name = base::ASCIIToUTF16("short_name");
  info_.UpdateFromManifest(manifest_);

  ASSERT_EQ(manifest_.short_name, info_.name);
}

TEST_F(ShortcutInfoTest, ShortNameFallsBackToName) {
  manifest_.name = base::ASCIIToUTF16("name");
  info_.UpdateFromManifest(manifest_);

  ASSERT_EQ(manifest_.name, info_.short_name);
}

TEST_F(ShortcutInfoTest, UserTitleBecomesShortName) {
  manifest_.short_name = base::ASCIIToUTF16("name");
  info_.user_title = base::ASCIIToUTF16("title");
  info_.UpdateFromManifest(manifest_);

  ASSERT_EQ(manifest_.short_name, info_.user_title);
}

// Test that if a manifest with an empty name and empty short_name is passed,
// that ShortcutInfo::UpdateFromManifest() does not overwrite the current
// ShortcutInfo::name and ShortcutInfo::short_name.
TEST_F(ShortcutInfoTest, IgnoreEmptyNameAndShortName) {
  base::string16 initial_name(base::ASCIIToUTF16("initial_name"));
  base::string16 initial_short_name(base::ASCIIToUTF16("initial_short_name"));

  info_.name = initial_name;
  info_.short_name = initial_short_name;
  manifest_.display = blink::mojom::DisplayMode::kStandalone;
  manifest_.name = base::string16();
  info_.UpdateFromManifest(manifest_);

  ASSERT_EQ(initial_name, info_.name);
  ASSERT_EQ(initial_short_name, info_.short_name);
}

TEST_F(ShortcutInfoTest, ShortcutItemsPopulated) {
  manifest_.shortcuts.push_back(CreateShortcut(
      "shortcut_1",
      {CreateImage("/i1_1", {16, 16}), CreateImage("/i1_2", {64, 64}),
       CreateImage("/i1_3", {192, 192}),  // best icon.
       CreateImage("/i1_4", {256, 256})}));

  manifest_.shortcuts.push_back(CreateShortcut(
      "shortcut_2", {CreateImage("/i2_1", {192, 194}),  // not square.
                     CreateImage("/i2_2", {194, 194})}));

  // Nothing chosen.
  manifest_.shortcuts.push_back(
      CreateShortcut("shortcut_3", {CreateImage("/i3_1", {16, 16})}));

  ShortcutHelper::SetIdealShortcutSizeForTesting(192);
  info_.UpdateFromManifest(manifest_);

  ASSERT_EQ(info_.best_shortcut_icon_urls.size(), 3u);
  EXPECT_EQ(info_.best_shortcut_icon_urls[0].path(), "/i1_3");
  EXPECT_EQ(info_.best_shortcut_icon_urls[1].path(), "/i2_2");
  EXPECT_FALSE(info_.best_shortcut_icon_urls[2].is_valid());
}

// Tests that if the optional shortcut short_name value is not provided, the
// required name value is used.
TEST_F(ShortcutInfoTest, ShortcutShortNameBackfilled) {
  // Create a shortcut without a |short_name|.
  manifest_.shortcuts.push_back(
      CreateShortcut(/* name= */ "name", /* icons= */ {}));

  info_.UpdateFromManifest(manifest_);

  ASSERT_EQ(info_.shortcut_items.size(), 1u);
  EXPECT_EQ(info_.shortcut_items[0].short_name, base::ASCIIToUTF16("name"));
}
