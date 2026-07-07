// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_selection_widget.h"

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget_observer.h"

namespace glic {

class GlicSelectionWidgetTest : public ChromeViewsTestBase {
 public:
  GlicSelectionWidgetTest() = default;
  ~GlicSelectionWidgetTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    animation_resetter_ = gfx::AnimationTestApi::SetRichAnimationRenderMode(
        gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
  }

  void TearDown() override {
    animation_resetter_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  void TriggerMenuCommand(GlicSelectionWidgetDelegate* delegate,
                          int command_id) {
    delegate->TriggerMenuCommandForTesting(command_id);
  }

 private:
  gfx::AnimationTestApi::RenderModeResetter animation_resetter_;
};

class TestWidgetObserver : public views::WidgetObserver {
 public:
  explicit TestWidgetObserver(views::Widget* widget) {
    observation_.Observe(widget);
  }
  ~TestWidgetObserver() override = default;

  void OnWidgetDestroyed(views::Widget* widget) override {
    observation_.Reset();
    widget_destroyed = true;
    if (quit_closure) {
      std::move(quit_closure).Run();
    }
  }

  bool widget_destroyed = false;
  base::OnceClosure quit_closure;

 private:
  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
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
  void OnHideForThisSite() override { hide_for_this_site_called = true; }
  void OnSettings() override { settings_called = true; }
  void OnWidgetClose() override { widget_close_called = true; }

  bool widget_close_called = false;

  bool ask_gemini_called = false;
  bool copy_called = false;
  bool copy_link_called = false;
  bool pin_toggled_called = false;
  bool pin_toggled_val = false;
  bool hide_for_this_site_called = false;
  bool settings_called = false;
};

TEST_F(GlicSelectionWidgetTest, CopyButtonsHiddenByDefault) {
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
  ASSERT_EQ(pill1_children.size(), 1u);
  EXPECT_TRUE(views::AsViewClass<views::MdTextButton>(pill1_children[0]));
}

TEST_F(GlicSelectionWidgetTest, ButtonsTriggerCallbacks) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kGlicSelectionPrompt,
      {{features::kGlicSelectionShowCopyButtons.name, "true"}});

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
  auto* menu_btn = views::AsViewClass<views::ImageButton>(pill2_children[0]);

  ASSERT_TRUE(ask_gemini_btn);
  ASSERT_TRUE(copy_btn);
  ASSERT_TRUE(copy_link_btn);
  ASSERT_TRUE(menu_btn);

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

  EXPECT_EQ(menu_btn->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_TOAST_MENU_BUTTON_NAME));

  views::test::ButtonTestApi(menu_btn).NotifyClick(
      ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON));

  // Test menu command execution.
  TriggerMenuCommand(
      widget_delegate.get(),
      static_cast<int>(GlicSelectionWidgetDelegate::MenuCommand::kHideForSite));
  EXPECT_TRUE(test_delegate->hide_for_this_site_called);

  TriggerMenuCommand(
      widget_delegate.get(),
      static_cast<int>(GlicSelectionWidgetDelegate::MenuCommand::kSettings));
  EXPECT_TRUE(test_delegate->settings_called);
}

TEST_F(GlicSelectionWidgetTest, ShowAndCloseWidget) {
  gfx::Rect anchor_rect(10, 10, 100, 100);
  std::u16string selected_text = u"selected text";

  auto test_delegate = std::make_unique<TestWidgetActionDelegate>();
  auto widget_delegate = std::make_unique<GlicSelectionWidgetDelegate>(
      *test_delegate, anchor_rect, gfx::Rect(), selected_text,
      /*is_pinned=*/false);

  EXPECT_FALSE(widget_delegate->GetWidget());

  std::unique_ptr<views::Widget> anchor_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  anchor_widget->Show();
  widget_delegate->set_parent_window(anchor_widget->GetNativeView());
  widget_delegate->ShowWidget();
  views::Widget* widget = widget_delegate->GetWidget();
  ASSERT_TRUE(widget);
  widget->Show();
  EXPECT_TRUE(widget->IsVisible());

  TestWidgetObserver observer(widget);
  base::RunLoop run_loop;
  observer.quit_closure = run_loop.QuitClosure();

  widget_delegate->CloseWidget();
  run_loop.Run();

  EXPECT_TRUE(observer.widget_destroyed);
  EXPECT_TRUE(test_delegate->widget_close_called);
}

}  // namespace glic
