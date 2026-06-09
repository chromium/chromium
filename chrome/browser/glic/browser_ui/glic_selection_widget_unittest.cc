// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_selection_widget.h"

#include "base/test/bind.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_utils.h"

namespace glic {

class GlicSelectionWidgetTest : public ChromeViewsTestBase {
 public:
  GlicSelectionWidgetTest() = default;
  ~GlicSelectionWidgetTest() override = default;
};

class TestWidgetActionDelegate
    : public GlicSelectionWidgetDelegate::ActionDelegate {
 public:
  void OnAskGemini() override { ask_gemini_called = true; }
  void OnCopy() override { copy_called = true; }
  void OnCopyLink() override { copy_link_called = true; }
  void OnPinToggled(bool is_pinned) override {
    pin_toggled_called = true;
    pin_toggled_val = is_pinned;
  }
  void OnDismiss() override { dismiss_called = true; }

  bool ask_gemini_called = false;
  bool copy_called = false;
  bool copy_link_called = false;
  bool pin_toggled_called = false;
  bool pin_toggled_val = false;
  bool dismiss_called = false;
};

TEST_F(GlicSelectionWidgetTest, ButtonsTriggerCallbacks) {
  gfx::Rect anchor_rect(10, 10, 100, 100);
  std::u16string selected_text = u"selected text";

  auto test_delegate = std::make_unique<TestWidgetActionDelegate>();
  auto widget_delegate = std::make_unique<GlicSelectionWidgetDelegate>(
      *test_delegate, anchor_rect, gfx::Rect(), selected_text,
      /*is_pinned=*/false);

  views::View* contents_view = widget_delegate->GetContentsView();
  ASSERT_TRUE(contents_view);

  auto children = contents_view->children();
  ASSERT_EQ(children.size(), 2u);

  auto pill1_children = children[0]->children();
  ASSERT_EQ(pill1_children.size(), 3u);

  auto* ask_gemini_btn =
      views::AsViewClass<views::MdTextButton>(pill1_children[0]);
  auto* copy_btn = views::AsViewClass<views::ImageButton>(pill1_children[1]);
  auto* copy_link_btn =
      views::AsViewClass<views::ImageButton>(pill1_children[2]);

  auto pill2_children = children[1]->children();
  ASSERT_EQ(pill2_children.size(), 1u);
  auto* dismiss_btn = views::AsViewClass<views::ImageButton>(pill2_children[0]);

  ASSERT_TRUE(ask_gemini_btn);
  ASSERT_TRUE(copy_btn);
  ASSERT_TRUE(copy_link_btn);

  // Verify the copy link button is initially disabled.
  EXPECT_FALSE(copy_link_btn->GetEnabled());

  // Enable it and test.
  widget_delegate->UpdateCopyLinkButton(true);
  EXPECT_TRUE(copy_link_btn->GetEnabled());

  // Manually run callbacks since we don't have a widget to receive events.
  views::test::ButtonTestApi(ask_gemini_btn)
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(),
                                  ui::EF_LEFT_MOUSE_BUTTON,
                                  ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_TRUE(test_delegate->ask_gemini_called);

  views::test::ButtonTestApi(copy_btn).NotifyClick(
      ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_TRUE(test_delegate->copy_called);

  views::test::ButtonTestApi(copy_link_btn)
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(),
                                  ui::EF_LEFT_MOUSE_BUTTON,
                                  ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_TRUE(test_delegate->copy_link_called);

  views::test::ButtonTestApi(dismiss_btn)
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(),
                                  ui::EF_LEFT_MOUSE_BUTTON,
                                  ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_TRUE(test_delegate->dismiss_called);
}

}  // namespace glic
