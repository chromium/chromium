// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/shortcut_info.h"

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "url/gurl.h"

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
  manifest_.name =
      base::NullableString16(base::ASCIIToUTF16("new name"), false);

  info_.short_name = base::ASCIIToUTF16("old short name");
  manifest_.short_name =
      base::NullableString16(base::ASCIIToUTF16("new short name"), false);

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

  ASSERT_EQ(manifest_.name.string(), info_.name);
  ASSERT_EQ(manifest_.short_name.string(), info_.short_name);
  ASSERT_EQ(manifest_.start_url, info_.url);
  ASSERT_EQ(manifest_.scope, info_.scope);
  ASSERT_EQ(manifest_.display, info_.display);
  ASSERT_EQ(manifest_.theme_color, info_.theme_color);
  ASSERT_EQ(manifest_.background_color, info_.background_color);
  ASSERT_EQ(1u, info_.icon_urls.size());
  ASSERT_EQ(manifest_.icons[0].src, GURL(info_.icon_urls[0]));
}

TEST_F(ShortcutInfoTest, NameFallsBackToShortName) {
  manifest_.short_name =
      base::NullableString16(base::ASCIIToUTF16("short_name"), false);
  info_.UpdateFromManifest(manifest_);

  ASSERT_EQ(manifest_.short_name.string(), info_.name);
}

TEST_F(ShortcutInfoTest, ShortNameFallsBackToName) {
  manifest_.name = base::NullableString16(base::ASCIIToUTF16("name"), false);
  info_.UpdateFromManifest(manifest_);

  ASSERT_EQ(manifest_.name.string(), info_.short_name);
}

TEST_F(ShortcutInfoTest, UserTitleBecomesShortName) {
  manifest_.short_name =
      base::NullableString16(base::ASCIIToUTF16("name"), false);
  info_.user_title = base::ASCIIToUTF16("title");
  info_.UpdateFromManifest(manifest_);

  ASSERT_EQ(manifest_.short_name.string(), info_.user_title);
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
  manifest_.name = base::NullableString16(base::string16(), false);
  info_.UpdateFromManifest(manifest_);

  ASSERT_EQ(initial_name, info_.name);
  ASSERT_EQ(initial_short_name, info_.short_name);
}
