// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/renderer_context_menu/context_menu_content_type_factory.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/renderer_context_menu/context_menu_content_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using extensions::MenuItem;

class ContextMenuContentTypeTest : public ChromeRenderViewHostTestHarness {
 public:
  static std::unique_ptr<ContextMenuContentType> Create(
      content::WebContents* web_contents,
      const content::ContextMenuParams& params) {
    return std::make_unique<ContextMenuContentType>(web_contents, params, true);
  }
};

// Generates a ContextMenuParams that matches the specified contexts.
content::ContextMenuParams CreateParams(int contexts) {
  content::ContextMenuParams rv;
  rv.is_editable = false;
  rv.media_type = blink::ContextMenuDataMediaType::kNone;
  rv.page_url = GURL("http://test.page/");

  static const base::string16 selected_text = base::ASCIIToUTF16("sel");
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
    rv.media_type = blink::ContextMenuDataMediaType::kImage;
  }

  if (contexts & MenuItem::VIDEO) {
    rv.src_url = GURL("http://test.video/");
    rv.media_type = blink::ContextMenuDataMediaType::kVideo;
  }

  if (contexts & MenuItem::AUDIO) {
    rv.src_url = GURL("http://test.audio/");
    rv.media_type = blink::ContextMenuDataMediaType::kAudio;
  }

  if (contexts & MenuItem::FRAME)
    rv.frame_url = GURL("http://test.frame/");

  return rv;
}

TEST_F(ContextMenuContentTypeTest, CheckTypes) {
  {
    content::ContextMenuParams params = CreateParams(MenuItem::LINK);
    std::unique_ptr<ContextMenuContentType> content_type(
        Create(web_contents(), params));
    EXPECT_TRUE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_LINK));
    EXPECT_TRUE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_ALL_EXTENSION));
    EXPECT_FALSE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_CURRENT_EXTENSION));
  }

  {
    content::ContextMenuParams params = CreateParams(MenuItem::SELECTION);
    std::unique_ptr<ContextMenuContentType> content_type(
        Create(web_contents(), params));
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
    std::unique_ptr<ContextMenuContentType> content_type(
        Create(web_contents(), params));
    EXPECT_FALSE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_LINK));
    EXPECT_FALSE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_COPY));
    EXPECT_TRUE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_EDITABLE));
  }

  {
    content::ContextMenuParams params = CreateParams(MenuItem::IMAGE);
    std::unique_ptr<ContextMenuContentType> content_type(
        Create(web_contents(), params));
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
    std::unique_ptr<ContextMenuContentType> content_type(
        Create(web_contents(), params));
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
    std::unique_ptr<ContextMenuContentType> content_type(
        Create(web_contents(), params));
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
    std::unique_ptr<ContextMenuContentType> content_type(
        Create(web_contents(), params));
    EXPECT_TRUE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_FRAME));
    EXPECT_TRUE(content_type->SupportsGroup(
                    ContextMenuContentType::ITEM_GROUP_PAGE));
  }
}
