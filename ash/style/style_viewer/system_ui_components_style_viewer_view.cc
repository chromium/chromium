// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/style_viewer/system_ui_components_style_viewer_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/shell.h"
#include "ash/style/style_viewer/system_ui_components_grid_view.h"
#include "ash/style/style_viewer/system_ui_components_grid_view_factories.h"
#include "ash/wm/desks/desks_util.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// The width and height of viewer contents.
constexpr int kContentWidth = 960;
constexpr int kContentHeight = 496;
constexpr int kBottomSpacing = 16;
// The width of components menu.
constexpr int kMenuWidth = 160;
// The height of component button.
constexpr int kDefaultButtonHeight = 32;
// The background color id of active component button.
constexpr ui::ColorId kActiveButtonBackgroundColorId =
    cros_tokens::kCrosSysPrimary;
// The text color id of active component button.
constexpr ui::ColorId kActiveButtonTextColorId = cros_tokens::kCrosSysOnPrimary;
// The background color id of inactive component button.
constexpr ui::ColorId kInactiveButtonBackgroundColorId =
    cros_tokens::kCrosSysSystemOnBase;
// The text color id of inactive component button.
constexpr ui::ColorId kInactiveButtonTextColorId =
    cros_tokens::kCrosSysOnSurface;

class SystemUIComponentsStyleViewerClientView : public views::ClientView {
 public:
  SystemUIComponentsStyleViewerClientView(views::Widget* widget,
                                          views::View* contents_view)
      : views::ClientView(widget, contents_view) {}

  SystemUIComponentsStyleViewerClientView(
      const SystemUIComponentsStyleViewerClientView&) = delete;
  SystemUIComponentsStyleViewerClientView& operator=(
      const SystemUIComponentsStyleViewerClientView&) = delete;

  ~SystemUIComponentsStyleViewerClientView() override = default;

  // ClientView:
  void UpdateWindowRoundedCorners(int corner_radius) override {
    //  The top corners will be rounded by NonClientFrameViewAsh. The
    // client-view is responsible for rounding the bottom corners.

    const gfx::RoundedCornersF radii(0, 0, corner_radius, corner_radius);
    contents_view()->SetBackground(views::CreateThemedRoundedRectBackground(
        ui::kColorDialogBackground, radii));
  }
};

}  // namespace

// The global singleton of the viewer widget.
static views::Widget* g_instance = nullptr;

// -----------------------------------------------------------------------------
// SystemUIComponentsStyleViewerView::ComponentButton:
class SystemUIComponentsStyleViewerView::ComponentButton
    : public views::LabelButton {
  METADATA_HEADER(ComponentButton, views::LabelButton)

 public:
  ComponentButton(views::LabelButton::PressedCallback pressed_callback,
                  const std::u16string& name)
      : views::LabelButton(std::move(pressed_callback), name) {
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
    SetBorder(std::make_unique<views::HighlightBorder>(
        0, chromeos::features::IsJellyrollEnabled()
               ? views::HighlightBorder::Type::kHighlightBorderNoShadow
               : views::HighlightBorder::Type::kHighlightBorder1));
    label()->SetSubpixelRenderingEnabled(false);
    label()->SetFontList(views::Label::GetDefaultFontList().Derive(
        1, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
    SetFocusBehavior(views::View::FocusBehavior::NEVER);
  }
  ComponentButton(const ComponentButton&) = delete;
  ComponentButton& operator=(const ComponentButton&) = delete;
  ~ComponentButton() override = default;

  void SetActive(bool active) {
    background_color_id_ = active ? kActiveButtonBackgroundColorId
                                  : kInactiveButtonBackgroundColorId;
    text_color_id_ =
        active ? kActiveButtonTextColorId : kInactiveButtonTextColorId;
    OnThemeChanged();
  }

  // views::LabelButton:
  void AddedToWidget() override {
    SetBackground(views::CreateSolidBackground(
        GetColorProvider()->GetColor(background_color_id_)));
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(kMenuWidth, kDefaultButtonHeight);
  }

  void OnThemeChanged() override {
    views::LabelButton::OnThemeChanged();

    if (!GetWidget())
      return;

    ui::ColorProvider* color_provider = GetColorProvider();
    SetEnabledTextColors(color_provider->GetColor(text_color_id_));
    if (auto* bg = background())
      bg->SetNativeControlColor(color_provider->GetColor(background_color_id_));
  }

 private:
  ui::ColorId background_color_id_ = kInactiveButtonBackgroundColorId;
  ui::ColorId text_color_id_ = kInactiveButtonTextColorId;
};

BEGIN_METADATA(SystemUIComponentsStyleViewerView, ComponentButton)
END_METADATA

// -----------------------------------------------------------------------------
// SystemUIComponentsStyleViewerView:
SystemUIComponentsStyleViewerView::SystemUIComponentsStyleViewerView()
    : menu_scroll_view_(
          AddChildView(views::ScrollView::CreateScrollViewWithBorder())),
      component_instances_scroll_view_(
          AddChildView(std::make_unique<views::ScrollView>())) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  SetBackground(views::CreateThemedSolidBackground(ui::kColorDialogBackground));
  SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(0, 0, kBottomSpacing, 0)));

  // Set menu scroll view.
  menu_scroll_view_->SetPreferredSize(gfx::Size(kMenuWidth, kContentHeight));
  menu_contents_view_ =
      menu_scroll_view_->SetContents(std::make_unique<views::View>());
  menu_contents_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  component_instances_scroll_view_->SetPreferredSize(
      gfx::Size(kContentWidth - kMenuWidth, kContentHeight));
  components_grid_view_ = component_instances_scroll_view_->SetContents(
      std::make_unique<views::View>());
}

SystemUIComponentsStyleViewerView::~SystemUIComponentsStyleViewerView() =
    default;

// static.
void SystemUIComponentsStyleViewerView::CreateAndShowWidget() {
  // Only create widget when there is no running instance.
  if (g_instance)
    return;

  // Owned by widget.
  SystemUIComponentsStyleViewerView* viewer_view =
      new SystemUIComponentsStyleViewerView();
  viewer_view->SetOwnedByWidget(true);

  viewer_view->AddComponent(
      u"PillButton", base::BindRepeating(&CreatePillButtonInstancesGirdView));
  viewer_view->AddComponent(
      u"IconButton", base::BindRepeating(&CreateIconButtonInstancesGridView));
  viewer_view->AddComponent(
      u"Checkbox", base::BindRepeating(&CreateCheckboxInstancesGridView));
  viewer_view->AddComponent(
      u"CheckboxGroup",
      base::BindRepeating(&CreateCheckboxGroupInstancesGridView));
  viewer_view->AddComponent(
      u"RadioButton", base::BindRepeating(&CreateRadioButtonInstancesGridView));
  viewer_view->AddComponent(
      u"RadioButtonGroup",
      base::BindRepeating(&CreateRadioButtonGroupInstancesGridView));
  viewer_view->AddComponent(
      u"Switch", base::BindRepeating(&CreateSwitchInstancesGridView));
  viewer_view->AddComponent(
      u"TabSlider", base::BindRepeating(&CreateTabSliderInstancesGridView));
  viewer_view->AddComponent(
      u"System Textfield",
      base::BindRepeating(&CreateSystemTextfieldInstancesGridView));
  viewer_view->AddComponent(
      u"Pagination", base::BindRepeating(&CreatePaginationInstancesGridView));
  viewer_view->AddComponent(
      u"Combobox", base::BindRepeating(&CreateComboboxInstancesGridView));
  viewer_view->AddComponent(
      u"Typography", base::BindRepeating(&CreateTypographyInstancesGridView));
  viewer_view->AddComponent(u"Cutouts",
                            base::BindRepeating(&CreateCutoutsGridView));

  // Show PillButton on start.
  viewer_view->ShowComponentInstances(u"PillButton");

  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.parent =
      desks_util::GetActiveDeskContainerForRoot(Shell::GetPrimaryRootWindow());
  params.delegate = viewer_view;

  // The widget is owned by the native widget.
  g_instance = new views::Widget(std::move(params));
  g_instance->AddObserver(viewer_view);
  g_instance->Show();
}

void SystemUIComponentsStyleViewerView::AddComponent(
    const std::u16string& name,
    SystemUIComponentsStyleViewerView::ComponentsGridViewFactory
        grid_view_factory) {
  DCHECK(!base::Contains(components_grid_view_factories_, name));

  // Add a new component button and components grid view factory.
  auto* button =
      menu_contents_view_->AddChildView(std::make_unique<ComponentButton>(
          base::BindRepeating(
              &SystemUIComponentsStyleViewerView::ShowComponentInstances,
              base::Unretained(this), name),
          name));
  buttons_.push_back(button);
  components_grid_view_factories_[name] = grid_view_factory;
}

void SystemUIComponentsStyleViewerView::ShowComponentInstances(
    const std::u16string& name) {
  DCHECK(base::Contains(components_grid_view_factories_, name));

  // Set the button corresponding to the component indicated by the name active.
  // Set other buttons inactive.
  for (ash::SystemUIComponentsStyleViewerView::ComponentButton* button :
       buttons_) {
    button->SetActive(button->GetText() == name);
  }

  // Toggle corresponding components grid view.
  components_grid_view_ = nullptr;
  components_grid_view_ = component_instances_scroll_view_->SetContents(
      components_grid_view_factories_[name].Run());
}

void SystemUIComponentsStyleViewerView::Layout(PassKey) {
  menu_contents_view_->SetSize(
      gfx::Size(kMenuWidth, menu_contents_view_->GetPreferredSize().height()));
  components_grid_view_->SizeToPreferredSize();
  LayoutSuperclass<views::View>(this);
}

std::u16string SystemUIComponentsStyleViewerView::GetWindowTitle() const {
  return u"System Components Style Viewer";
}

views::ClientView* SystemUIComponentsStyleViewerView::CreateClientView(
    views::Widget* widget) {
  return new SystemUIComponentsStyleViewerClientView(widget, this);
}

void SystemUIComponentsStyleViewerView::OnWidgetDestroyed(
    views::Widget* widget) {
  g_instance = nullptr;
}

BEGIN_METADATA(SystemUIComponentsStyleViewerView)
END_METADATA

}  // namespace ash
