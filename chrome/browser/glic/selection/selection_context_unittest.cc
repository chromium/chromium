// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/selection_context.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace glic {
namespace {

std::string GetSelectedText(const mojom::AdditionalContext& context) {
  const mojom::ContextData& data = *context.parts[0]->get_data();
  EXPECT_EQ(data.mime_type, kMimeTypeGlicSelection);
  return std::string(data.data.begin(), data.data.end());
}

using SelectionContextTest = ChromeRenderViewHostTestHarness;

TEST_F(SelectionContextTest, TextSelectionElided) {
  const std::u16string text(200, u'a');
  mojom::AdditionalContextPtr context = CreateTextSelectionContext(
      web_contents(), text, SelectionTextLength::kElided);

  EXPECT_EQ(context->source, mojom::AdditionalContextSource::kTextSelection);
  ASSERT_EQ(context->parts.size(), 1u);
  std::string sent = GetSelectedText(*context);
  EXPECT_LT(sent.size(), base::UTF16ToUTF8(text).size());
  EXPECT_LE(base::UTF8ToUTF16(sent).size(), 100u);
}

TEST_F(SelectionContextTest, TextSelectionFull) {
  const std::u16string text(200, u'a');
  mojom::AdditionalContextPtr context = CreateTextSelectionContext(
      web_contents(), text, SelectionTextLength::kFull);

  ASSERT_EQ(context->parts.size(), 1u);
  EXPECT_EQ(GetSelectedText(*context), base::UTF16ToUTF8(text));
}

TEST_F(SelectionContextTest, TextSelectionEmpty) {
  mojom::AdditionalContextPtr context = CreateTextSelectionContext(
      web_contents(), u"", SelectionTextLength::kFull);

  EXPECT_EQ(context->source, mojom::AdditionalContextSource::kTextSelection);
  EXPECT_TRUE(context->parts.empty());
}

TEST(SelectionContextRegionTest, RegionSelection) {
  tabs::MockTabInterface tab;
  const base::UnguessableToken id = base::UnguessableToken::Create();
  const gfx::Rect rect(10, 20, 30, 40);
  CapturedRegions regions;
  regions.emplace_back(id, mojom::CapturedRegion::NewRect(rect));

  mojom::AdditionalContextPtr context = CreateRegionSelectionContext(
      tab, mojom::TabContextResult::New(), std::move(regions));

  EXPECT_EQ(context->source, mojom::AdditionalContextSource::kRegionSelection);
  EXPECT_EQ(context->tab_id, tab.GetHandle().raw_value());
  ASSERT_EQ(context->parts.size(), 2u);
  EXPECT_TRUE(context->parts[0]->is_tab_context());
  ASSERT_TRUE(context->parts[1]->is_pending_region());
  EXPECT_EQ(context->parts[1]->get_pending_region()->id, id);
  EXPECT_EQ(context->parts[1]->get_pending_region()->region->get_rect(), rect);
}

}  // namespace
}  // namespace glic
