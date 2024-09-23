// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/drop_down_checkbox.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/checkbox.h"
#include "ash/style/checkbox_group.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "ash/wm/work_area_insets.h"
#include "base/functional/bind.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/list_model_observer.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_target.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The color constants.
constexpr ui::ColorId kActiveTitleAndIconColorId =
    cros_tokens::kCrosSysSystemOnPrimaryContainer;
constexpr ui::ColorId kInactiveTitleAndIconColorId =
    cros_tokens::kCrosSysOnSurface;
constexpr ui::ColorId kMenuTextColorId = cros_tokens::kCrosSysOnSurface;
constexpr ui::ColorId kMenuBackgroundColorId =
    cros_tokens::kCrosSysSystemBaseElevated;
constexpr ui::ColorId kDropDownCheckboxActiveColorId =
    cros_tokens::kCrosSysSystemPrimaryContainer;

// The layout parameters.
constexpr int kDropDownCheckboxRoundedCorners = 12;
constexpr int kMenuRoundedCorners = 12;
constexpr gfx::Insets kDropDownCheckboxBorderInsets =
    gfx::Insets::TLBR(4, 10, 4, 4);
constexpr gfx::Insets kMenuBorderInsets = gfx::Insets::TLBR(16, 0, 12, 0);
constexpr gfx::Insets kMenuItemInnerPadding = gfx::Insets::VH(8, 16);
constexpr int kArrowIconSize = 20;
constexpr int kCheckmarkLabelSpacing = 16;
constexpr int kMaxMenuWidth = 168;
constexpr gfx::Vector2d kMenuOffset(0, 8);
constexpr int kMenuShadowElevation = 12;

class CheckboxMenuOptionGroup : public CheckboxGroup {
  METADATA_HEADER(CheckboxMenuOptionGroup, CheckboxGroup)

 public:
  CheckboxMenuOptionGroup()
      : CheckboxGroup(kMaxMenuWidth,
                      kMenuBorderInsets,
                      0,
                      kMenuItemInnerPadding,
                      kCheckmarkLabelSpacing) {
    GetViewAccessibility().SetRole(ax::mojom::Role::kListBox);
  }

  // CheckboxGroup:
  Checkbox* AddButton(Checkbox::PressedCallback callback,
                      const std::u16string& label) override {
    auto* button = AddChildView(std::make_unique<Checkbox>(
        group_width_ - inside_border_insets_.width(), std::move(callback),
        label, kMenuItemInnerPadding, kCheckmarkLabelSpacing));
    button->set_delegate(this);
    buttons_.push_back(button);
    return button;
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    CheckboxGroup::GetAccessibleNodeData(node_data);
    node_data->SetNameExplicitlyEmpty();
  }
};

BEGIN_METADATA(CheckboxMenuOptionGroup)
END_METADATA

}  // namespace

//------------------------------------------------------------------------------
// DropDownCheckbox::SelectionModel:
class DropDownCheckbox::SelectionModel : public ui::ListSelectionModel,
                                         public ui::ListModelObserver {
 public:
  SelectionModel() = default;
  SelectionModel(const SelectionModel&) = delete;
  SelectionModel& operator=(const SelectionModel&) = delete;
  ~SelectionModel() override = default;

  // ui::ListModelObserver:
  void ListItemsAdded(size_t start, size_t count) override {
    for (size_t i = 0; i < count; i++) {
      IncrementFrom(start + i);
    }
  }

  void ListItemsRemoved(size_t start, size_t count) override {
    for (size_t i = 0; i < count; i++) {
      DecrementFrom(start);
    }
  }

  void ListItemMoved(size_t index, size_t target_index) override {
    DecrementFrom(index);
    IncrementFrom(target_index);
  }

  void ListItemsChanged(size_t start, size_t count) override {}
};

//------------------------------------------------------------------------------
// DropDownCheckbox::MenuView:
class DropDownCheckbox::MenuView : public views::View {
  METADATA_HEADER(MenuView, views::View)

 public:
  explicit MenuView(DropDownCheckbox* drop_down_check_box)
      : drop_down_checkbox_(drop_down_check_box) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    menu_item_group_ =
        AddChildView(std::make_unique<CheckboxMenuOptionGroup>());
    UpdateMenuContent();
    SetBackground(views::CreateThemedRoundedRectBackground(
        kMenuBackgroundColorId, kMenuRoundedCorners));
    // Set border.
    SetBorder(std::make_unique<views::HighlightBorder>(
        kMenuRoundedCorners,
        views::HighlightBorder::Type::kHighlightBorderOnShadow));
  }
  MenuView(const MenuView&) = delete;
  MenuView& operator=(const MenuView&) = delete;
  ~MenuView() override = default;

  void UpdateMenuContent() {
    menu_item_group_->RemoveAllChildViews();

    // Build a checkbox group according to current list model.
    for (size_t i = 0; i < drop_down_checkbox_->model_->item_count(); i++) {
      auto* item = menu_item_group_->AddButton(
          base::BindRepeating(&DropDownCheckbox::MenuView::OnItemSelected,
                              base::Unretained(this), i),
          *drop_down_checkbox_->model_->GetItemAt(i));
      item->SetLabelStyle(TypographyToken::kCrosButton2);
      item->SetLabelColorId(kMenuTextColorId);
      item->SetSelected(drop_down_checkbox_->selection_model_->IsSelected(i));
    }
  }

 private:
  void OnItemSelected(size_t index) {
    auto* selection_model = drop_down_checkbox_->selection_model_.get();
    if (selection_model->IsSelected(index)) {
      selection_model->RemoveIndexFromSelection(index);
    } else {
      selection_model->AddIndexToSelection(index);
    }
  }

  const raw_ptr<DropDownCheckbox> drop_down_checkbox_;
  // Owned by this.
  raw_ptr<CheckboxMenuOptionGroup> menu_item_group_;
};

BEGIN_METADATA(DropDownCheckbox, MenuView)
END_METADATA

//------------------------------------------------------------------------------
// DropDownCheckbox::EventHandler:
// Handles the mouse and touch event that happens outside drop down checkbox and
// its drop down menu.
class DropDownCheckbox::EventHandler : public ui::EventHandler {
 public:
  explicit EventHandler(DropDownCheckbox* drop_down_checkbox)
      : drop_down_checkbox_(drop_down_checkbox) {
    aura::Env::GetInstance()->AddPreTargetHandler(
        this, ui::EventTarget::Priority::kSystem);
  }

  EventHandler(const EventHandler&) = delete;
  EventHandler& operator=(const EventHandler&) = delete;
  ~EventHandler() override {
    aura::Env::GetInstance()->RemovePreTargetHandler(this);
  }

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override { OnLocatedEvent(event); }

  void OnTouchEvent(ui::TouchEvent* event) override { OnLocatedEvent(event); }

 private:
  void OnLocatedEvent(ui::LocatedEvent* event) {
    // Close drop down menu if certain mouse or touch events happening outside
    // label button or menu area.
    if (!drop_down_checkbox_->IsMenuRunning()) {
      return;
    }

    // Get event location in screen.
    gfx::Point event_location = event->location();
    aura::Window* event_target = static_cast<aura::Window*>(event->target());
    wm::ConvertPointToScreen(event_target, &event_location);

    const bool event_in_drop_down_checkbox =
        drop_down_checkbox_->GetBoundsInScreen().Contains(event_location);
    const bool event_in_menu =
        drop_down_checkbox_->menu_->GetWindowBoundsInScreen().Contains(
            event_location);
    switch (event->type()) {
      case ui::EventType::kMousewheel:
        // Close menu if scrolling outside menu.
        if (!event_in_menu) {
          drop_down_checkbox_->CloseDropDownMenu();
        }
        break;
      case ui::EventType::kMousePressed:
      case ui::EventType::kTouchPressed:
        // Close menu if pressing outside menu and label button.
        if (!event_in_menu && !event_in_drop_down_checkbox) {
          event->StopPropagation();
          drop_down_checkbox_->CloseDropDownMenu();
        }
        break;
      default:
        break;
    }
  }

  const raw_ptr<DropDownCheckbox> drop_down_checkbox_;
};

//------------------------------------------------------------------------------
// DropDownCheckbox:
DropDownCheckbox::DropDownCheckbox(const std::u16string& title,
                                   DropDownCheckbox::ItemModel* model)
    : views::Button(
          base::BindRepeating(&DropDownCheckbox::OnDropDownCheckboxPressed,
                              base::Unretained(this))),
      model_(model),
      title_(AddChildView(std::make_unique<views::Label>(title))),
      drop_down_arrow_(AddChildView(std::make_unique<views::ImageView>(
          ui::ImageModel::FromVectorIcon(kDropDownArrowIcon,
                                         kInactiveTitleAndIconColorId,
                                         kArrowIconSize)))),
      selection_model_(std::make_unique<SelectionModel>()) {
  // Initialize the drop down menu with given model.
  CHECK(model_);
  model_->AddObserver(selection_model_.get());

  // Set up layout.
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetInteriorMargin(kDropDownCheckboxBorderInsets);
  // Allow `title_` to shrink and elide, so that `drop_down_arrow_` on the
  // right always remains visible.
  title_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));

  // Stylize the title.
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosTitle1,
                                        *title_.get());
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetEnabledColorId(kInactiveTitleAndIconColorId);

  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // Set up the ink drop.
  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(), kDropDownCheckboxRoundedCorners);
  StyleUtil::SetUpInkDropForButton(this);
  views::FocusRing::Get(this)->SetProperty(views::kViewIgnoredByLayoutKey,
                                           /*ignored=*/true);

  event_handler_ = std::make_unique<EventHandler>(this);

  GetViewAccessibility().SetRole(ax::mojom::Role::kPopUpButton);
}

DropDownCheckbox::~DropDownCheckbox() = default;

void DropDownCheckbox::SetSelectedAction(base::RepeatingClosure callback) {
  callback_ = std::move(callback);
}

DropDownCheckbox::SelectedIndices DropDownCheckbox::GetSelectedIndices() const {
  return selection_model_->selected_indices();
}

DropDownCheckbox::SelectedItems DropDownCheckbox::GetSelectedItems() const {
  SelectedItems selected_items;
  for (size_t index : GetSelectedIndices()) {
    selected_items.push_back(*model_->GetItemAt(index));
  }
  return selected_items;
}

bool DropDownCheckbox::IsMenuRunning() const {
  return !!menu_;
}

void DropDownCheckbox::SetCallback(PressedCallback callback) {
  NOTREACHED() << "Clients shouldn't modify this. Maybe you want to use "
                  "SetSelectedAction?";
}

void DropDownCheckbox::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // Move menu with combobox accordingly.
  if (menu_) {
    menu_->SetBounds(GetExpectedMenuBounds());
  }
}

void DropDownCheckbox::OnBlur() {
  if (menu_) {
    CloseDropDownMenu();
  }

  views::Button::OnBlur();
}

void DropDownCheckbox::AddedToWidget() {
  widget_observer_.Observe(GetWidget());
}

void DropDownCheckbox::RemovedFromWidget() {
  widget_observer_.Reset();
}

void DropDownCheckbox::Layout(PassKey) {
  LayoutSuperclass<views::Button>(this);
  views::FocusRing::Get(this)->DeprecatedLayoutImmediately();
}

void DropDownCheckbox::OnWidgetBoundsChanged(views::Widget* widget,
                                             const gfx::Rect& bounds) {
  if (menu_) {
    menu_->SetBounds(GetExpectedMenuBounds());
  }
}

gfx::Rect DropDownCheckbox::GetExpectedMenuBounds() const {
  CHECK(menu_view_);
  WorkAreaInsets* work_area =
      WorkAreaInsets::ForWindow(GetWidget()->GetNativeWindow());
  const gfx::Rect available_bounds = work_area->user_work_area_bounds();

  const gfx::Size preferred_size = menu_view_->GetPreferredSize();
  const gfx::Rect drop_down_checkbox_bounds = GetBoundsInScreen();

  // Decide whether to show the menu below (default) or above the label button:
  // if the menu fits below the label button, show it below.
  const int height_below = available_bounds.bottom() -
                           drop_down_checkbox_bounds.bottom() - kMenuOffset.y();
  bool show_below_drop_down_checkbox = height_below >= preferred_size.height();
  // If the drop down menu does not fit below label button, show it above the
  // label button of there is more space available above.
  if (!show_below_drop_down_checkbox) {
    const int height_above =
        drop_down_checkbox_bounds.y() - available_bounds.y() - kMenuOffset.y();
    show_below_drop_down_checkbox = height_below >= height_above;
  }

  gfx::Rect preferred_bounds =
      show_below_drop_down_checkbox
          ? gfx::Rect(drop_down_checkbox_bounds.bottom_left() + kMenuOffset,
                      preferred_size)
          : gfx::Rect(
                drop_down_checkbox_bounds.origin() +
                    gfx::Vector2d(kMenuOffset.x(),
                                  -preferred_size.height() - kMenuOffset.y()),
                preferred_size);

  // If the label button is offscreen, translate the preferred bounds to fit
  // available bounds.
  if (show_below_drop_down_checkbox &&
      drop_down_checkbox_bounds.bottom() < available_bounds.y()) {
    preferred_bounds.Offset(
        0, available_bounds.y() - drop_down_checkbox_bounds.bottom());
  } else if (!show_below_drop_down_checkbox &&
             drop_down_checkbox_bounds.y() > available_bounds.bottom()) {
    preferred_bounds.Offset(
        0, available_bounds.bottom() - drop_down_checkbox_bounds.y());
  }

  preferred_bounds.Intersect(available_bounds);
  return preferred_bounds;
}

void DropDownCheckbox::OnDropDownCheckboxPressed() {
  if (!GetEnabled()) {
    return;
  }

  if (menu_) {
    CloseDropDownMenu();
  } else if ((base::TimeTicks::Now() - closed_time_) >
             views::kMinimumTimeBetweenButtonClicks) {
    ShowDropDownMenu();
  }
}

void DropDownCheckbox::ShowDropDownMenu() {
  auto* widget = GetWidget();
  if (!widget) {
    return;
  }

  auto menu_view = std::make_unique<MenuView>(this);
  menu_view_ = menu_view.get();

  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.shadow_elevation = kMenuShadowElevation;
  params.corner_radius = kMenuRoundedCorners;

  aura::Window* root_window = widget->GetNativeWindow()->GetRootWindow();
  params.parent = root_window->GetChildById(kShellWindowId_MenuContainer);
  params.bounds = GetExpectedMenuBounds();

  menu_ = std::make_unique<views::Widget>(std::move(params));
  menu_->SetContentsView(std::move(menu_view));
  menu_->Show();

  SetBackground(views::CreateThemedRoundedRectBackground(
      kDropDownCheckboxActiveColorId, kDropDownCheckboxRoundedCorners));
  title_->SetEnabledColorId(kActiveTitleAndIconColorId);
  drop_down_arrow_->SetImage(ui::ImageModel::FromVectorIcon(
      kDropDownArrowIcon, kActiveTitleAndIconColorId, kArrowIconSize));

  RequestFocus();
  NotifyAccessibilityEvent(ax::mojom::Event::kStateChanged, true);
}

void DropDownCheckbox::CloseDropDownMenu() {
  menu_view_ = nullptr;
  menu_.reset();

  closed_time_ = base::TimeTicks::Now();
  SetBackground(nullptr);
  title_->SetEnabledColorId(kInactiveTitleAndIconColorId);
  drop_down_arrow_->SetImage(ui::ImageModel::FromVectorIcon(
      kDropDownArrowIcon, kInactiveTitleAndIconColorId, kArrowIconSize));
  NotifyAccessibilityEvent(ax::mojom::Event::kStateChanged, true);
  OnPerformAction();
}

void DropDownCheckbox::OnPerformAction() {
  if (callback_) {
    callback_.Run();
  }
}

BEGIN_METADATA(DropDownCheckbox)
END_METADATA

}  // namespace ash
