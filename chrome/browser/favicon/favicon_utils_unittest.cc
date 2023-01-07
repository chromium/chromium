// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_utils.h"

#include "content/public/browser/navigation_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace favicon {

TEST(FaviconUtilsTest, ShouldThemifyFavicon) {
  std::unique_ptr<content::NavigationEntry> entry =
      content::NavigationEntry::Create();
  const GURL unthemeable_url("http://mail.google.com");
  const GURL themeable_virtual_url("chrome://feedback/");
  const GURL themeable_url("chrome://new-tab-page/");

  entry->SetVirtualURL(themeable_virtual_url);
  entry->SetURL(themeable_url);
  // Entry should be themefied if both its virtual and actual URLs are
  // themeable.
  EXPECT_TRUE(ShouldThemifyFaviconForEntry(entry.get()));

  entry->SetVirtualURL(unthemeable_url);
  // Entry should be themefied if only its actual URL is themeable.
  EXPECT_TRUE(ShouldThemifyFaviconForEntry(entry.get()));

  entry->SetURL(unthemeable_url);
  // Entry should not be themefied if both its virtual and actual URLs are
  // not themeable.
  EXPECT_FALSE(ShouldThemifyFaviconForEntry(entry.get()));

  entry->SetVirtualURL(themeable_virtual_url);
  // Entry should be themefied if only its virtual URL is themeable.
  EXPECT_TRUE(ShouldThemifyFaviconForEntry(entry.get()));
}

}  // namespace favicon
