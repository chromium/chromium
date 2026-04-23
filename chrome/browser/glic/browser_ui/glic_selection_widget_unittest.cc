// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_selection_widget.h"

#include "base/test/bind.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_utils.h"

namespace glic {

class GlicSelectionWidgetTest : public ChromeViewsTestBase {
 public:
  GlicSelectionWidgetTest() = default;
  ~GlicSelectionWidgetTest() override = default;
};

TEST_F(GlicSelectionWidgetTest, ButtonsTriggerCallbacks) {
  bool ask_gemini_called = false;
  bool copy_called = false;
  bool copy_link_called = false;

  gfx::Rect anchor_rect(10, 10, 100, 100);
  std::u16string selected_text = u"selected text";

  GlicSelectionWidgetDelegate* delegate = new GlicSelectionWidgetDelegate(
      anchor_rect, selected_text,
      base::BindLambdaForTesting([&]() { ask_gemini_called = true; }),
      base::BindLambdaForTesting([&]() { copy_called = true; }),
      base::BindLambdaForTesting([&]() { copy_link_called = true; }));

  views::View* contents_view = delegate->GetContentsView();
  ASSERT_TRUE(contents_view);

  auto children = contents_view->children();
  ASSERT_EQ(children.size(), 3u);

  auto* ask_gemini_btn = views::AsViewClass<views::ImageButton>(children[0]);
  auto* copy_btn = views::AsViewClass<views::ImageButton>(children[1]);
  auto* copy_link_btn = views::AsViewClass<views::ImageButton>(children[2]);

  ASSERT_TRUE(ask_gemini_btn);
  ASSERT_TRUE(copy_btn);
  ASSERT_TRUE(copy_link_btn);

  // Verify the copy link button is initially disabled.
  EXPECT_FALSE(copy_link_btn->GetEnabled());

  // Enable it and test.
  delegate->UpdateCopyLinkButton(true);
  EXPECT_TRUE(copy_link_btn->GetEnabled());

  // Manually run callbacks since we don't have a widget to receive events.
  views::test::ButtonTestApi(ask_gemini_btn)
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(),
                                  ui::EF_LEFT_MOUSE_BUTTON,
                                  ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_TRUE(ask_gemini_called);

  views::test::ButtonTestApi(copy_btn).NotifyClick(
      ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_TRUE(copy_called);

  views::test::ButtonTestApi(copy_link_btn)
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(),
                                  ui::EF_LEFT_MOUSE_BUTTON,
                                  ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_TRUE(copy_link_called);

  delete delegate;
}

}  // namespace glic
