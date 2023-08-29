// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/renderer_context_menu/context_menu_content_type_factory.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/renderer_context_menu/context_menu_content_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom-shared.h"
#include "url/gurl.h"

using extensions::MenuItem;

using ContextMenuContentTypeTest = ChromeRenderViewHostTestHarness;

// Generates a ContextMenuParams that matches the specified contexts.
content::ContextMenuParams CreateParams(int contexts) {
  content::ContextMenuParams rv;
  rv.is_editable = false;
  rv.media_type = blink::mojom::ContextMenuDataMediaType::kNone;
  rv.page_url = GURL("http://test.page/");
  rv.frame_url = rv.page_url;

  static const std::u16string selected_text = u"sel";
  if (contexts & MenuItem::SELECTION)
    rv.selection_text = selected_text;

  if (contexts & MenuItem::LINK) {
    rv.link_url = GURL("http://test.link/");
    rv.unfiltered_link_url = GURL("http://test.link/");
  }

  if (contexts & MenuItem::EDITABLE)
    rv.is_editable = true;

  if (contexts & MenuItem::IMAGE) {
    rv.src_url = GURL("http://test.image/");
    rv.media_type = blink::mojom::ContextMenuDataMediaType::kImage;
  }

  if (contexts & MenuItem::VIDEO) {
    rv.src_url = GURL("http://test.video/");
    rv.media_type = blink::mojom::ContextMenuDataMediaType::kVideo;
  }

  if (contexts & MenuItem::AUDIO) {
    rv.src_url = GURL("http://test.audio/");
    rv.media_type = blink::mojom::ContextMenuDataMediaType::kAudio;
  }

  if (contexts & MenuItem::FRAME)
    rv.is_subframe = true;

  return rv;
}

TEST_F(ContextMenuContentTypeTest, CheckTypes) {
  {
    content::ContextMenuParams params = CreateParams(MenuItem::LINK);
    auto content_type = std::make_unique<ContextMenuContentType>(params, true);
    EXPECT_TRUE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_LINK));
    EXPECT_TRUE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_ALL_EXTENSION));
    EXPECT_FALSE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_CURRENT_EXTENSION));
  }

  {
    content::ContextMenuParams params = CreateParams(MenuItem::SELECTION);
    auto content_type = std::make_unique<ContextMenuContentType>(params, true);
    EXPECT_FALSE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_LINK));
    EXPECT_TRUE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_COPY));
    EXPECT_FALSE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_EDITABLE));
    EXPECT_TRUE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_SEARCH_PROVIDER));
  }

  {
    content::ContextMenuParams params = CreateParams(MenuItem::EDITABLE);
    auto content_type = std::make_unique<ContextMenuContentType>(params, true);
    EXPECT_FALSE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_LINK));
    EXPECT_FALSE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_COPY));
    EXPECT_TRUE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_EDITABLE));
  }

  {
    content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);
    auto content_type = std::make_unique<ContextMenuContentType>(params, true);
    EXPECT_TRUE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_MEDIA_IMAGE));
    EXPECT_TRUE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_SEARCHWEBFORIMAGE));
    EXPECT_TRUE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_PRINT));

    EXPECT_FALSE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_MEDIA_VIDEO));
    EXPECT_FALSE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_MEDIA_AUDIO));
    EXPECT_FALSE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_MEDIA_PLUGIN));
  }

  {
    content::ContextMenuParams params = CreateParams(MenuItem::VIDEO);
    auto content_type = std::make_unique<ContextMenuContentType>(params, true);
    EXPECT_TRUE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_MEDIA_VIDEO));

    EXPECT_FALSE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_MEDIA_IMAGE));
    EXPECT_FALSE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_MEDIA_AUDIO));
    EXPECT_FALSE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_MEDIA_PLUGIN));
  }

  {
    content::ContextMenuParams params = CreateParams(MenuItem::AUDIO);
    auto content_type = std::make_unique<ContextMenuContentType>(params, true);
    EXPECT_TRUE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_MEDIA_AUDIO));

    EXPECT_FALSE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_MEDIA_IMAGE));
    EXPECT_FALSE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_MEDIA_VIDEO));
    EXPECT_FALSE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_MEDIA_PLUGIN));
  }

  {
    content::ContextMenuParams params = CreateParams(MenuItem::FRAME);
    auto content_type = std::make_unique<ContextMenuContentType>(params, true);
    EXPECT_TRUE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_FRAME));
    EXPECT_TRUE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_PAGE));
  }
}
