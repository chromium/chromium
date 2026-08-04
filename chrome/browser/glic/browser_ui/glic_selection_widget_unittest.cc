// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_selection_widget.h"

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/menus/simple_menu_model.h"
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
  void OnAskGeminiForQuery(const std::u16string& query) override {}
  void OnAskGeminiMoreAboutThis(
      const std::u16string& selected_text,
      const std::string& explanation_text) override {}
  void OnAskGeminiWithSkill(
      const GlicSelectionWidgetDelegate::SkillOption& skill) override {
    ask_gemini_with_skill_called = true;
    last_invoked_skill = skill;
  }
  std::vector<GlicSelectionWidgetDelegate::SkillOption> GetContextualSkills()
      override {
    return contextual_skills;
  }
  std::vector<GlicSelectionWidgetDelegate::SkillOption> GetUserSkills()
      override {
    return user_skills;
  }
  void OnCopy() override { copy_called = true; }
  void OnCopyLink() override { copy_link_called = true; }
  void OnHide() override { hide_called = true; }
  void OnSettings() override { settings_called = true; }
  void OnOpenInSidePanel() override {}
  void OnWidgetClose() override { widget_close_called = true; }
  bool IsInlineFulfillmentSupported() override {
    return inline_fulfillment_supported;
  }

  std::vector<GlicSelectionWidgetDelegate::SkillOption> contextual_skills;
  std::vector<GlicSelectionWidgetDelegate::SkillOption> user_skills;
  bool ask_gemini_with_skill_called = false;
  GlicSelectionWidgetDelegate::SkillOption last_invoked_skill;
  bool inline_fulfillment_supported = false;
  bool widget_close_called = false;

  bool ask_gemini_called = false;
  bool copy_called = false;
  bool copy_link_called = false;
  bool hide_called = false;
  bool settings_called = false;
};

TEST_F(GlicSelectionWidgetTest, CopyButtonsHiddenByDefault) {
  gfx::Rect anchor_rect(10, 10, 100, 100);
  std::u16string selected_text = u"selected text";

  auto test_delegate = std::make_unique<TestWidgetActionDelegate>();
  auto widget_delegate = std::make_unique<GlicSelectionWidgetDelegate>(
      *test_delegate, anchor_rect, gfx::Rect(), selected_text);

  views::View* contents_view = widget_delegate->GetContentsView();
  ASSERT_TRUE(contents_view);

  auto children = contents_view->children();
  ASSERT_EQ(children.size(), 1u);

  auto pill_children = children[0]->children();
  ASSERT_EQ(pill_children.size(), 2u);
  EXPECT_TRUE(views::AsViewClass<views::MdTextButton>(pill_children[0]));

  auto close_children = pill_children[1]->children();
  ASSERT_EQ(close_children.size(), 1u);
  EXPECT_TRUE(views::AsViewClass<views::ImageButton>(close_children[0]));
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
      *test_delegate, anchor_rect, gfx::Rect(), selected_text);

  views::View* contents_view = widget_delegate->GetContentsView();
  ASSERT_TRUE(contents_view);

  auto children = contents_view->children();
  ASSERT_EQ(children.size(), 1u);

  auto pill_children = children[0]->children();
  ASSERT_EQ(pill_children.size(), 4u);

  auto* ask_gemini_btn =
      views::AsViewClass<views::MdTextButton>(pill_children[0]);
  auto* copy_btn = views::AsViewClass<views::ImageButton>(pill_children[1]);
  auto* copy_link_btn =
      views::AsViewClass<views::ImageButton>(pill_children[2]);

  views::View* close_pill = pill_children[3];
  auto close_children = close_pill->children();
  ASSERT_EQ(close_children.size(), 1u);
  auto* close_btn = views::AsViewClass<views::ImageButton>(close_children[0]);

  ASSERT_TRUE(ask_gemini_btn);
  ASSERT_TRUE(copy_btn);
  ASSERT_TRUE(copy_link_btn);
  ASSERT_TRUE(close_btn);

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

  EXPECT_EQ(close_btn->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_CLOSE));

  views::test::ButtonTestApi(close_btn).NotifyClick(
      ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_TRUE(test_delegate->hide_called);
}

TEST_F(GlicSelectionWidgetTest, ShowAndCloseWidget) {
  gfx::Rect anchor_rect(10, 10, 100, 100);
  std::u16string selected_text = u"selected text";

  auto test_delegate = std::make_unique<TestWidgetActionDelegate>();
  auto widget_delegate = std::make_unique<GlicSelectionWidgetDelegate>(
      *test_delegate, anchor_rect, gfx::Rect(), selected_text);

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

TEST_F(GlicSelectionWidgetTest,
       AskGeminiButtonDoesNotStartExpansionTimerWhenInlineFulfillmentDisabled) {
  gfx::Rect anchor_rect(10, 10, 100, 100);
  std::u16string selected_text = u"selected text";

  auto test_delegate = std::make_unique<TestWidgetActionDelegate>();
  test_delegate->inline_fulfillment_supported = false;
  auto widget_delegate = std::make_unique<GlicSelectionWidgetDelegate>(
      *test_delegate, anchor_rect, gfx::Rect(), selected_text);

  views::View* contents_view = widget_delegate->GetContentsView();
  ASSERT_TRUE(contents_view);

  auto children = contents_view->children();
  ASSERT_EQ(children.size(), 1u);

  auto pill_children = children[0]->children();
  auto* ask_gemini_btn =
      views::AsViewClass<views::MdTextButton>(pill_children[0]);
  ASSERT_TRUE(ask_gemini_btn);

  views::test::ButtonTestApi(ask_gemini_btn)
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(),
                                  ui::EF_LEFT_MOUSE_BUTTON,
                                  ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_TRUE(test_delegate->ask_gemini_called);

  task_environment()->FastForwardBy(base::Milliseconds(200));

  EXPECT_EQ(contents_view->children().size(), 1u);
  EXPECT_TRUE(children[0]->GetVisible());
}

TEST_F(GlicSelectionWidgetTest,
       AskGeminiButtonStartsExpansionTimerWhenInlineFulfillmentEnabled) {
  gfx::Rect anchor_rect(10, 10, 100, 100);
  std::u16string selected_text = u"selected text";

  auto test_delegate = std::make_unique<TestWidgetActionDelegate>();
  test_delegate->inline_fulfillment_supported = true;
  auto widget_delegate = std::make_unique<GlicSelectionWidgetDelegate>(
      *test_delegate, anchor_rect, gfx::Rect(), selected_text);

  views::View* contents_view = widget_delegate->GetContentsView();
  ASSERT_TRUE(contents_view);

  auto children = contents_view->children();
  ASSERT_EQ(children.size(), 1u);

  auto pill_children = children[0]->children();
  auto* ask_gemini_btn =
      views::AsViewClass<views::MdTextButton>(pill_children[0]);
  ASSERT_TRUE(ask_gemini_btn);

  views::test::ButtonTestApi(ask_gemini_btn)
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(),
                                  ui::EF_LEFT_MOUSE_BUTTON,
                                  ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_TRUE(test_delegate->ask_gemini_called);
  EXPECT_EQ(contents_view->children().size(), 1u);
  EXPECT_TRUE(children[0]->GetVisible());

  task_environment()->FastForwardBy(base::Milliseconds(200));

  EXPECT_EQ(contents_view->children().size(), 2u);
  EXPECT_FALSE(children[0]->GetVisible());
}

// Disabled on Mac because Mac's native menu is synchronous.
#if !BUILDFLAG(IS_MAC)
TEST_F(GlicSelectionWidgetTest, AskGeminiRightClickSkillsMenu) {
  gfx::Rect anchor_rect(10, 10, 100, 100);
  std::u16string selected_text = u"selected text";

  auto test_delegate = std::make_unique<TestWidgetActionDelegate>();
  test_delegate->contextual_skills.emplace_back(
      skills::Skill("context_1", "Contextual Skill 1", "💼", ""));
  test_delegate->user_skills.emplace_back(
      skills::Skill("user_1", "User Skill 1", "🧪", ""));
  test_delegate->user_skills.emplace_back(
      skills::Skill("user_2", "User Skill 2", "", ""));

  auto widget_delegate = std::make_unique<GlicSelectionWidgetDelegate>(
      *test_delegate, anchor_rect, gfx::Rect(), selected_text);

  std::unique_ptr<views::Widget> anchor_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  anchor_widget->Show();
  widget_delegate->set_parent_window(anchor_widget->GetNativeView());
  widget_delegate->ShowWidget();
  views::Widget* widget = widget_delegate->GetWidget();
  ASSERT_TRUE(widget);
  widget->Show();

  views::View* ask_gemini_btn =
      widget_delegate->GetAskGeminiButtonForTesting();
  ASSERT_TRUE(ask_gemini_btn);

  EXPECT_FALSE(widget_delegate->IsContextMenuShowingForTesting());

  ask_gemini_btn->ShowContextMenu(
      ask_gemini_btn->GetBoundsInScreen().CenterPoint(),
      ui::mojom::MenuSourceType::kMouse);

  EXPECT_TRUE(widget_delegate->IsContextMenuShowingForTesting());

  ui::SimpleMenuModel* menu_model =
      widget_delegate->GetContextMenuModelForTesting();
  ASSERT_TRUE(menu_model);

  // Index 0: title "Your skills"
  // Index 1: user_1 (command_id 100)
  // Index 2: user_2 (command_id 101)
  // Index 3: separator
  // Index 4: title "For this page"
  // Index 5: context_1 (command_id 102)
  EXPECT_EQ(menu_model->GetItemCount(), 6u);
  EXPECT_EQ(menu_model->GetLabelAt(0),
            l10n_util::GetStringUTF16(IDS_GLIC_SELECTION_YOUR_SKILLS));
  EXPECT_EQ(menu_model->GetCommandIdAt(1),
            GlicSelectionWidgetDelegate::kMinSkillCommandId);
  EXPECT_EQ(menu_model->GetLabelAt(1), u"🧪 User Skill 1");
  EXPECT_EQ(menu_model->GetLabelAt(2), u"User Skill 2");
  EXPECT_EQ(menu_model->GetLabelAt(4),
            l10n_util::GetStringUTF16(IDS_GLIC_SELECTION_FOR_THIS_PAGE));
  EXPECT_EQ(menu_model->GetLabelAt(5), u"💼 Contextual Skill 1");

  menu_model->ActivatedAt(1);
  EXPECT_TRUE(test_delegate->ask_gemini_with_skill_called);
  EXPECT_EQ(test_delegate->last_invoked_skill.id, "user_1");
  EXPECT_EQ(test_delegate->last_invoked_skill.name, "User Skill 1");

  TestWidgetObserver observer(widget);
  base::RunLoop run_loop;
  observer.quit_closure = run_loop.QuitClosure();

  widget_delegate->CloseWidget();

  run_loop.Run();
}

TEST_F(GlicSelectionWidgetTest, AskGeminiRightClickMoreSkillsSubmenu) {
  gfx::Rect anchor_rect(10, 10, 100, 100);
  std::u16string selected_text = u"selected text";

  auto test_delegate = std::make_unique<TestWidgetActionDelegate>();
  test_delegate->user_skills.emplace_back(
      skills::Skill("user_1", "User Skill 1", "", ""));
  test_delegate->user_skills.emplace_back(
      skills::Skill("user_2", "User Skill 2", "", ""));
  test_delegate->user_skills.emplace_back(
      skills::Skill("user_3", "User Skill 3", "", ""));
  test_delegate->user_skills.emplace_back(
      skills::Skill("user_4", "User Skill 4", "🎨", ""));

  auto widget_delegate = std::make_unique<GlicSelectionWidgetDelegate>(
      *test_delegate, anchor_rect, gfx::Rect(), selected_text);

  std::unique_ptr<views::Widget> anchor_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  anchor_widget->Show();
  widget_delegate->set_parent_window(anchor_widget->GetNativeView());
  widget_delegate->ShowWidget();
  views::Widget* widget = widget_delegate->GetWidget();
  ASSERT_TRUE(widget);
  widget->Show();

  views::View* ask_gemini_btn =
      widget_delegate->GetAskGeminiButtonForTesting();
  ASSERT_TRUE(ask_gemini_btn);

  ask_gemini_btn->ShowContextMenu(
      ask_gemini_btn->GetBoundsInScreen().CenterPoint(),
      ui::mojom::MenuSourceType::kMouse);

  ui::SimpleMenuModel* menu_model =
      widget_delegate->GetContextMenuModelForTesting();
  ASSERT_TRUE(menu_model);

  // 1 title "Your skills" + 2 top-level user skills + 1 "More" submenu = 4 items
  EXPECT_EQ(menu_model->GetItemCount(), 4u);
  EXPECT_EQ(menu_model->GetLabelAt(0),
            l10n_util::GetStringUTF16(IDS_GLIC_SELECTION_YOUR_SKILLS));
  EXPECT_EQ(menu_model->GetLabelAt(3),
            l10n_util::GetStringUTF16(IDS_GLIC_SELECTION_MORE_SKILLS));
  ui::MenuModel* submenu_model = menu_model->GetSubmenuModelAt(3);
  ASSERT_TRUE(submenu_model);
  EXPECT_EQ(submenu_model->GetItemCount(), 2u);
  EXPECT_EQ(submenu_model->GetLabelAt(1), u"🎨 User Skill 4");

  submenu_model->ActivatedAt(1);
  EXPECT_TRUE(test_delegate->ask_gemini_with_skill_called);
  EXPECT_EQ(test_delegate->last_invoked_skill.id, "user_4");
  EXPECT_EQ(test_delegate->last_invoked_skill.name, "User Skill 4");

  TestWidgetObserver observer(widget);
  base::RunLoop run_loop;
  observer.quit_closure = run_loop.QuitClosure();

  widget_delegate->CloseWidget();

  run_loop.Run();
}

TEST_F(GlicSelectionWidgetTest,
       AskGeminiRightClickSkillsMenuDisabledByFeatureParam) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kGlicSelectionPrompt, {{"skills", "false"}});

  gfx::Rect anchor_rect(10, 10, 100, 100);
  std::u16string selected_text = u"selected text";

  auto test_delegate = std::make_unique<TestWidgetActionDelegate>();
  test_delegate->user_skills.emplace_back(
      skills::Skill("user_1", "User Skill 1", "🧪", ""));

  auto widget_delegate = std::make_unique<GlicSelectionWidgetDelegate>(
      *test_delegate, anchor_rect, gfx::Rect(), selected_text);

  std::unique_ptr<views::Widget> anchor_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  anchor_widget->Show();
  widget_delegate->set_parent_window(anchor_widget->GetNativeView());
  widget_delegate->ShowWidget();
  views::Widget* widget = widget_delegate->GetWidget();
  ASSERT_TRUE(widget);
  widget->Show();

  views::View* ask_gemini_btn =
      widget_delegate->GetAskGeminiButtonForTesting();
  ASSERT_TRUE(ask_gemini_btn);

  ask_gemini_btn->ShowContextMenu(
      ask_gemini_btn->GetBoundsInScreen().CenterPoint(),
      ui::mojom::MenuSourceType::kMouse);

  EXPECT_FALSE(widget_delegate->IsContextMenuShowingForTesting());

  TestWidgetObserver observer(widget);
  base::RunLoop run_loop;
  observer.quit_closure = run_loop.QuitClosure();

  widget_delegate->CloseWidget();

  run_loop.Run();
}
#endif  // !BUILDFLAG(IS_MAC)

}  // namespace glic
