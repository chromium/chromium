// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <memory>
#include <string>

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/style/checkbox_group.h"
#include "ash/style/icon_button.h"
#include "ash/style/radio_button_group.h"
#include "ash/style/system_dialog_delegate_view.h"
#include "ash/style/tab_slider.h"
#include "ash/style/tab_slider_button.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_test_api.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

const gfx::VectorIcon& kTestIcon = kFolderIcon;

enum class TabSliderType {
  kIconSlider,
  kLabelSlider,
  kIconLabelSlider,
};

// Helpers ---------------------------------------------------------------------

std::unique_ptr<views::Widget> CreateSystemDialogWidget(
    ui::mojom::ModalType modal_type,
    aura::Window* parent_window) {
  // Generate a new dialog delegate view.
  auto dialog_view = views::Builder<SystemDialogDelegateView>()
                         .SetIcon(kTestIcon)
                         .SetTitleText(u"Title")
                         .SetDescription(u"Dialog description.")
                         .Build();

  dialog_view->SetModalType(modal_type);

  // Create a dialog widget.
  views::Widget::InitParams dialog_params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  dialog_params.bounds = gfx::Rect(dialog_view->GetPreferredSize());
  dialog_params.delegate = dialog_view.release();
  dialog_params.parent = parent_window;
  auto dialog_widget =
      std::make_unique<views::Widget>(std::move(dialog_params));
  dialog_widget->Show();
  return dialog_widget;
}

// WidgetWithSystemUIComponentView ---------------------------------------------

// A WidgetDelegateView with given component as the only content.
class WidgetWithSystemUIComponentView : public views::WidgetDelegateView {
 public:
  explicit WidgetWithSystemUIComponentView(
      std::unique_ptr<views::View> component) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    AddChildView(std::move(component));
  }
  WidgetWithSystemUIComponentView(const WidgetWithSystemUIComponentView&) =
      delete;
  WidgetWithSystemUIComponentView& operator=(
      const WidgetWithSystemUIComponentView&) = delete;
  ~WidgetWithSystemUIComponentView() override = default;
};

// Creates a test widget with given component as the only content.
std::unique_ptr<views::Widget> CreateWidgetWithComponent(
    std::unique_ptr<views::View> component) {
  return AshTestBase::CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      new WidgetWithSystemUIComponentView(std::move(component)));
}

}  // namespace

using SystemComponentsTest = AshTestBase;

// Tests if the tooltip text of PillButton is same with the button text, unless
// the tooltip text is explicitly set.
TEST_F(SystemComponentsTest, PillButtonTooltip) {
  // Create a PillButton object.
  auto pill_button =
      std::make_unique<PillButton>(PillButton::PressedCallback(), u"Default");

  // The tooltip text should be same with initial button text.
  EXPECT_EQ(pill_button->GetTooltipText(gfx::Point()), u"Default");

  // Changing button text will also update the tooltip text.
  pill_button->SetText(u"New Text");
  EXPECT_EQ(pill_button->GetTooltipText(gfx::Point()), u"New Text");

  // If the tooltip text is explicitly set, the tooltip text will always be use.
  pill_button->SetTooltipText(u"Tooltip");
  EXPECT_EQ(pill_button->GetTooltipText(gfx::Point()), u"Tooltip");

  // Updating button text won't change the preset tooltip text.
  pill_button->SetText(u"Foo");
  EXPECT_EQ(pill_button->GetTooltipText(gfx::Point()), u"Tooltip");
}

// TODO(crbug.com/40878458): Disable for constant failure.
TEST_F(SystemComponentsTest,
       DISABLED_IconButtonWithBackgroundColorIdDoesNotCrash) {
  // Create an IconButton with an explicit background color ID.
  auto icon_button = std::make_unique<IconButton>(
      IconButton::PressedCallback(), IconButton::Type::kSmall, &kTestIcon,
      u"button 1",
      /*is_toggleable=*/false, /*has_border=*/false);
  auto* icon_button_ptr = icon_button.get();
  icon_button->SetBackgroundColor(cros_tokens::kCrosSysSystemOnBase);
  auto widget = CreateWidgetWithComponent(std::move(icon_button));

  // Schedule a paint for the button.
  icon_button_ptr->SchedulePaint();
  EXPECT_TRUE(views::ViewTestApi(icon_button_ptr).needs_paint());

  // Spin the message loop so the button paints.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(views::ViewTestApi(icon_button_ptr).needs_paint());

  // No crash.
}

// Tests that when one button is selected in the radio button group, the others
// will be unselected automatically.
TEST_F(SystemComponentsTest, RadioButtonGroup) {
  std::unique_ptr<RadioButtonGroup> radio_button_group =
      std::make_unique<RadioButtonGroup>(198);

  // Add three buttons to the group.
  auto* button_1 = radio_button_group->AddButton(RadioButton::PressedCallback(),
                                                 u"Test Button1");
  auto* button_2 = radio_button_group->AddButton(RadioButton::PressedCallback(),
                                                 u"Test Button2");
  auto* button_3 = radio_button_group->AddButton(RadioButton::PressedCallback(),
                                                 u"Test Button3");

  auto* switch_raw_ptr = radio_button_group.get();
  auto widget = CreateWidgetWithComponent(std::move(radio_button_group));

  // All the buttons should be in unselected state.
  EXPECT_FALSE(button_1->selected());
  EXPECT_FALSE(button_2->selected());
  EXPECT_FALSE(button_3->selected());

  // select the first button by using `RadioButton::SetSelected`.
  button_1->SetSelected(true);
  // Only the first button is selected.
  EXPECT_TRUE(button_1->selected());
  EXPECT_FALSE(button_2->selected());
  EXPECT_FALSE(button_3->selected());

  // Select the second button by mouse clicking.
  LeftClickOn(button_2);
  // Only the second button is selected.
  EXPECT_FALSE(button_1->selected());
  EXPECT_TRUE(button_2->selected());
  EXPECT_FALSE(button_3->selected());

  // Select the third button by using `RadioButtonGroup::SelectButtonAtIndex`.
  switch_raw_ptr->SelectButtonAtIndex(2);
  // Only the third button is selected.
  EXPECT_FALSE(button_1->selected());
  EXPECT_FALSE(button_2->selected());
  EXPECT_TRUE(button_3->selected());

  // Using `SetSelected` again on the first button will unselect the other
  // buttons.
  button_1->SetSelected(true);
  // Only the first button is selected.
  EXPECT_TRUE(button_1->selected());
  EXPECT_FALSE(button_2->selected());
  EXPECT_FALSE(button_3->selected());

  // Disabling radio button group makes all radio buttons disabled.
  switch_raw_ptr->SetEnabled(false);
  EXPECT_FALSE(button_1->GetEnabled());
  EXPECT_FALSE(button_2->GetEnabled());
  EXPECT_FALSE(button_3->GetEnabled());
}

// Tests that all buttons in the checkbox group can be selected / unselected.
// Click on a selected button will unselect, and vice versa。
TEST_F(SystemComponentsTest, CheckboxGroup) {
  std::unique_ptr<CheckboxGroup> checkbox_group =
      std::make_unique<CheckboxGroup>(198);

  // Add four buttons to the group.
  auto* button_1 =
      checkbox_group->AddButton(Checkbox::PressedCallback(), u"Test Button1");
  auto* button_2 =
      checkbox_group->AddButton(Checkbox::PressedCallback(), u"Test Button2");
  auto* button_3 =
      checkbox_group->AddButton(Checkbox::PressedCallback(), u"Test Button3");
  auto* button_4 =
      checkbox_group->AddButton(Checkbox::PressedCallback(), u"Test Button4");

  auto* switch_raw_ptr = checkbox_group.get();
  auto widget = CreateWidgetWithComponent(std::move(checkbox_group));

  // All the buttons should be in unselected state.
  EXPECT_FALSE(button_1->selected());
  EXPECT_FALSE(button_2->selected());
  EXPECT_FALSE(button_3->selected());
  EXPECT_FALSE(button_4->selected());

  // select the first button by using `Checkbox::SetSelected`.
  button_1->SetSelected(true);
  // The first button is selected.
  EXPECT_TRUE(button_1->selected());
  EXPECT_FALSE(button_2->selected());
  EXPECT_FALSE(button_3->selected());
  EXPECT_FALSE(button_4->selected());

  // Select the second button by mouse clicking.
  LeftClickOn(button_2);
  // The first and second buttons are selected.
  EXPECT_TRUE(button_1->selected());
  EXPECT_TRUE(button_2->selected());
  EXPECT_FALSE(button_3->selected());
  EXPECT_FALSE(button_4->selected());

  // Click on the second button again, it should be unselected.
  LeftClickOn(button_2);
  EXPECT_FALSE(button_2->selected());

  // Select the third button by using `CheckboxGroup::SelectButtonAtIndex`.
  switch_raw_ptr->SelectButtonAtIndex(2);
  // The third button should be selected.
  EXPECT_TRUE(button_3->selected());

  // Using `SetSelected` on the fourth button.
  button_4->SetSelected(true);
  // All buttons should be selected except the second one.
  EXPECT_TRUE(button_1->selected());
  EXPECT_FALSE(button_2->selected());
  EXPECT_TRUE(button_3->selected());
  EXPECT_TRUE(button_4->selected());

  // Disabling radio button group makes all radio buttons disabled.
  switch_raw_ptr->SetEnabled(false);
  EXPECT_FALSE(button_1->GetEnabled());
  EXPECT_FALSE(button_2->GetEnabled());
  EXPECT_FALSE(button_3->GetEnabled());
  EXPECT_FALSE(button_4->GetEnabled());
}

TEST_F(SystemComponentsTest, AccessibleDefaultActionVerb) {
  std::unique_ptr<RadioButtonGroup> radio_button_group =
      std::make_unique<RadioButtonGroup>(198);
  auto* button = radio_button_group->AddButton(RadioButton::PressedCallback(),
                                               u"Test Button");
  ui::AXNodeData data;

  // Enabled
  button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetDefaultActionVerb(), ax::mojom::DefaultActionVerb::kCheck);

  button->SetSelected(true);
  data = ui::AXNodeData();
  button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetDefaultActionVerb(),
            ax::mojom::DefaultActionVerb::kUncheck);

  button->SetSelected(false);
  data = ui::AXNodeData();
  button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetDefaultActionVerb(), ax::mojom::DefaultActionVerb::kCheck);

  // Disabled
  button->SetEnabled(false);
  button->SetSelected(true);
  data = ui::AXNodeData();
  button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_FALSE(
      data.HasIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb));

  button->SetSelected(false);
  data = ui::AXNodeData();
  button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_FALSE(
      data.HasIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb));
}

struct DialogTestParams {
  ui::mojom::ModalType modal_type;
  bool parent_to_root;
};

using SystemDialogDelegateViewTest = SystemComponentsTest;

TEST_F(SystemDialogDelegateViewTest, CancelCallback) {
  std::unique_ptr<views::Widget> dialog_widget =
      CreateSystemDialogWidget(ui::mojom::ModalType::kNone,
                               /*parent_window=*/Shell::GetPrimaryRootWindow());
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, accept_callback);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, cancel_callback);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, close_callback);

  auto* const system_dialog_delegate_view =
      static_cast<SystemDialogDelegateView*>(dialog_widget->GetContentsView());
  system_dialog_delegate_view->SetAcceptCallback(accept_callback.Get());
  system_dialog_delegate_view->SetCancelCallback(cancel_callback.Get());
  system_dialog_delegate_view->SetCloseCallback(close_callback.Get());

  // Close the dialog through the cancel button. Only `cancel_callback` should
  // be executed.
  EXPECT_CALL_IN_SCOPE(cancel_callback, Run, {
    auto* const cancel_button = system_dialog_delegate_view->GetViewByID(
        ViewID::VIEW_ID_STYLE_SYSTEM_DIALOG_DELEGATE_CANCEL_BUTTON);
    ASSERT_TRUE(cancel_button);
    LeftClickOn(cancel_button);
    views::test::WidgetDestroyedWaiter(system_dialog_delegate_view->GetWidget())
        .Wait();
  });
}

// Verifies that the close callback registered on `SystemDialogDelegateView`
// should run when the dialog view is destroyed without clicking any buttons.
TEST_F(SystemDialogDelegateViewTest, CloseCallback) {
  std::unique_ptr<views::Widget> dialog_widget =
      CreateSystemDialogWidget(ui::mojom::ModalType::kNone,
                               /*parent_window=*/Shell::GetPrimaryRootWindow());
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, accept_callback);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, cancel_callback);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, close_callback);

  auto* const system_dialog_delegate_view =
      static_cast<SystemDialogDelegateView*>(dialog_widget->GetContentsView());
  system_dialog_delegate_view->SetAcceptCallback(accept_callback.Get());
  system_dialog_delegate_view->SetCancelCallback(cancel_callback.Get());
  system_dialog_delegate_view->SetCloseCallback(close_callback.Get());
  EXPECT_CALL_IN_SCOPE(close_callback, Run, dialog_widget.reset());
}

class SystemDialogSizeTest
    : public SystemComponentsTest,
      public testing::WithParamInterface<DialogTestParams> {
 public:
  SystemDialogSizeTest() = default;
  SystemDialogSizeTest(const SystemDialogSizeTest&) = delete;
  SystemDialogSizeTest& operator=(const SystemDialogSizeTest&) = delete;
  ~SystemDialogSizeTest() override = default;

 protected:
  // Create a dialog according to the give test parameters. Resize the host
  // window with the given host size.
  void CreateDialog(const DialogTestParams& params,
                    const gfx::Size& host_size) {
    // Clear existing dialog and host window instances.
    dialog_.reset();
    host_widget_.reset();

    // Resize the display if the dialog is parented to the root window.
    // Otherwise, create a host window with the given size.
    if (params.parent_to_root) {
      UpdateDisplay(host_size.ToString());
    } else {
      UpdateDisplay("1280x720");
      host_widget_ =
          CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                           nullptr, desks_util::GetActiveDeskContainerId(),
                           gfx::Rect(host_size), /*show=*/true);
    }

    dialog_ = CreateSystemDialogWidget(params.modal_type,
                                       params.parent_to_root
                                           ? Shell::GetPrimaryRootWindow()
                                           : host_widget_->GetNativeWindow());
  }

  // Get the dialog size.
  int GetDialogWidth() {
    CHECK(dialog_);
    return dialog_->GetWindowBoundsInScreen().size().width();
  }

 private:
  std::unique_ptr<views::Widget> host_widget_;
  std::unique_ptr<views::Widget> dialog_;
};

const DialogTestParams kSystemDialogTestParams[] = {
    {ui::mojom::ModalType::kNone, /*parent_to_root=*/false},
    {ui::mojom::ModalType::kNone, /*parent_to_root=*/true},
    {ui::mojom::ModalType::kWindow, /*parent_to_root=*/false},
    {ui::mojom::ModalType::kWindow, /*parent_to_root=*/true},
    {ui::mojom::ModalType::kChild, /*parent_to_root=*/false},
    {ui::mojom::ModalType::kChild, /*parent_to_root=*/true},
    {ui::mojom::ModalType::kSystem, /*parent_to_root=*/false},
    {ui::mojom::ModalType::kSystem, /*parent_to_root=*/true},
};

INSTANTIATE_TEST_SUITE_P(SystemDialogSize,
                         SystemDialogSizeTest,
                         testing::ValuesIn(kSystemDialogTestParams));

// Tests the dialog sizes with different sizes of host windows.
TEST_P(SystemDialogSizeTest, DialogResponsiveSize) {
  DialogTestParams params = GetParam();

  // When the width of the host window is no smaller than 672, the width of the
  // dialog is 512.
  CreateDialog(params, /*host_size=*/gfx::Size(1000, 600));
  EXPECT_EQ(512, GetDialogWidth());

  CreateDialog(params, /*host_size=*/gfx::Size(672, 600));
  EXPECT_EQ(512, GetDialogWidth());

  // When the width of the host window is less than 672 and no smaller than 520,
  // the dialog has a padding of 80 on both sides.
  CreateDialog(params, /*host_size=*/gfx::Size(671, 600));
  EXPECT_EQ(511, GetDialogWidth());

  CreateDialog(params, /*host_size=*/gfx::Size(520, 600));
  EXPECT_EQ(360, GetDialogWidth());

  // When the width of the host window is less than 520 and no smaller than 424,
  // the width of the dialog is 359.
  CreateDialog(params, /*host_size=*/gfx::Size(519, 600));
  EXPECT_EQ(359, GetDialogWidth());

  CreateDialog(params, /*host_size=*/gfx::Size(424, 600));
  EXPECT_EQ(359, GetDialogWidth());

  // When the width of the host window is less than 424 and no smaller than 400,
  // the dialog has a padding of 32 on both sides.
  CreateDialog(params, /*host_size=*/gfx::Size(423, 600));
  EXPECT_EQ(359, GetDialogWidth());

  CreateDialog(params, /*host_size=*/gfx::Size(400, 600));
  EXPECT_EQ(336, GetDialogWidth());

  // When the width of the host window is less than 400, the dialog of has
  // minimum size of 296.
  CreateDialog(params, /*host_size=*/gfx::Size(399, 600));
  EXPECT_EQ(296, GetDialogWidth());

  CreateDialog(params, /*host_size=*/gfx::Size(300, 600));
  EXPECT_EQ(296, GetDialogWidth());
}

class TabSliderTest : public SystemComponentsTest,
                      public testing::WithParamInterface<
                          std::tuple<TabSliderType, bool, bool, int>> {
 public:
  TabSliderTest() = default;
  TabSliderTest(const TabSliderTest&) = delete;
  TabSliderTest& operator=(const TabSliderTest&) = delete;
  ~TabSliderTest() override = default;
};

const TabSliderType kTabSliderTypes[] = {TabSliderType::kIconSlider,
                                         TabSliderType::kLabelSlider,
                                         TabSliderType::kIconLabelSlider};

INSTANTIATE_TEST_SUITE_P(
    TabSliderStyle,
    TabSliderTest,
    testing::Combine(
        /*tab slider types*/ testing::ValuesIn(kTabSliderTypes),
        /*distribute space evenly*/ testing::Bool(),
        /*use default layout*/ testing::Bool(),
        /*number of tabs*/ testing::Values(2, 3)),
    [](const testing::TestParamInfo<TabSliderTest::ParamType>& info) {
      std::string test_name;

      const TabSliderType type = std::get<0>(info.param);
      switch (type) {
        case TabSliderType::kIconSlider:
          test_name = "IconSlider";
          break;
        case TabSliderType::kLabelSlider:
          test_name = "LabelSlider";
          break;
        case TabSliderType::kIconLabelSlider:
          test_name = "IconLabelSlider";
          break;
      }

      test_name +=
          std::get<1>(info.param) ? "EvenlyDistributed" : "UnevenlyDistributed";
      test_name += std::get<2>(info.param) ? "DefaultLayout" : "CustomLayout";
      test_name += base::NumberToString(std::get<3>(info.param)) + "Buttons";
      return test_name;
    });

// Tests tab slider layout works properly with different layout settings.
TEST_P(TabSliderTest, TabSliderLayout) {
  auto test_params = GetParam();
  const TabSliderType type = std::get<0>(test_params);
  const bool distribute_space_evenly = std::get<1>(test_params);
  const bool use_default_layout = std::get<2>(test_params);
  const int tab_num = std::get<3>(test_params);

  TabSlider::InitParams params = (type == TabSliderType::kIconLabelSlider)
                                     ? IconLabelSliderButton::kSliderParams
                                     : TabSlider::kDefaultParams;

  params.distribute_space_evenly = distribute_space_evenly;

  if (!use_default_layout) {
    params.internal_border_padding = 3;
    params.between_buttons_spacing = 5;
  }

  // Create a tab slider.
  auto tab_slider =
      std::make_unique<TabSlider>(/*max_tab_num=*/tab_num, params);

  // The texts for tabs.
  const std::u16string labels_text[] = {u"one", u"one two three",
                                        u"one two three four five"};
  // Add slider buttons according to the testing parameters.
  std::vector<TabSliderButton*> buttons(tab_num, nullptr);
  int max_button_width = 0;
  int max_button_height = 0;
  for (int i = 0; i < tab_num; i++) {
    switch (type) {
      case TabSliderType::kIconSlider:
        buttons[i] = tab_slider->AddButton<IconSliderButton>(
            IconSliderButton::PressedCallback(), &kTestIcon,
            u"icon slider button");
        break;
      case TabSliderType::kLabelSlider:
        buttons[i] = tab_slider->AddButton<LabelSliderButton>(
            LabelSliderButton::PressedCallback(), labels_text[i],
            u"label slider button");
        break;
      case TabSliderType::kIconLabelSlider:
        buttons[i] = tab_slider->AddButton<IconLabelSliderButton>(
            IconLabelSliderButton::PressedCallback(), &kTestIcon,
            labels_text[i], u"icon label slider button");
        break;
    }

    // Cache the maximum size of the button.
    gfx::Size pref_size = buttons[i]->GetPreferredSize();
    max_button_width = std::max(max_button_width, pref_size.width());
    max_button_height = std::max(max_button_height, pref_size.height());
  }

  // Attach the tab slider to a widget.
  auto widget = CreateWidgetWithComponent(std::move(tab_slider));
  int x = params.internal_border_padding;
  const int y = params.internal_border_padding;
  // Check if the layout works properly.
  for (int i = 0; i < tab_num; i++) {
    const gfx::Size pref_size = buttons[i]->GetPreferredSize();
    const int expect_width =
        params.distribute_space_evenly ? max_button_width : pref_size.width();
    const int expect_height =
        params.distribute_space_evenly ? max_button_height : pref_size.height();
    EXPECT_EQ(buttons[i]->bounds(),
              gfx::Rect(x, y, expect_width, expect_height));
    x += expect_width + params.between_buttons_spacing;
  }
}

}  // namespace ash
