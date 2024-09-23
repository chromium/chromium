// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/combobox.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/blurred_background_shield.h"
#include "ash/style/radio_button_group.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "ash/wm/work_area_insets.h"
#include "base/functional/bind.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_target.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
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
constexpr ui::ColorId kComboboxActiveColorId =
    cros_tokens::kCrosSysSystemPrimaryContainer;

// The layout parameters.
constexpr gfx::RoundedCornersF kComboboxRoundedCorners =
    gfx::RoundedCornersF(12, 12, 12, 4);
constexpr gfx::RoundedCornersF kMenuRoundedCorners =
    gfx::RoundedCornersF(4, 12, 12, 12);
constexpr gfx::Insets kMenuBorderInsets = gfx::Insets::TLBR(16, 0, 12, 0);
constexpr gfx::Insets kMenuItemInnerPadding = gfx::Insets::VH(8, 16);
constexpr int kArrowIconSize = 20;
constexpr int kCheckmarkLabelSpacing = 16;
constexpr int kMaxMenuWidth = 168;
constexpr int kMaxMenuHeight = 172;
constexpr gfx::Vector2d kMenuOffset(0, 8);
constexpr int kMenuShadowElevation = 12;

class ComboboxMenuOption : public RadioButton {
  METADATA_HEADER(ComboboxMenuOption, RadioButton)

 public:
  ComboboxMenuOption(int button_width,
                     PressedCallback callback,
                     const std::u16string& label)
      : RadioButton(button_width,
                    std::move(callback),
                    label,
                    RadioButton::IconDirection::kLeading,
                    RadioButton::IconType::kCheck,
                    kMenuItemInnerPadding,
                    kCheckmarkLabelSpacing) {
    // The option is visually a radio button, but handles press actions more
    // like a button - when pressed, the combobox menu will be closed, and the
    // pressed option will get selected for the combobox. For this reason, for
    // accessibility, treat the menu option as a list box option instead of
    // radio button.
    GetViewAccessibility().SetRole(ax::mojom::Role::kListBoxOption);
    // Clear the checked state set by the base class. The check is used as an
    // indicator of the current combobox menu selection, and gets updated as the
    // keyboard selection changes. Announcing that each item that gets keyboard
    // selection is checked does not add value to the user and may cause
    // confusion. Additionally, if checked state is set, the action verb will
    // indicate that activating the item toggles it, which would be misleading.
    GetViewAccessibility().SetCheckedState(ax::mojom::CheckedState::kNone);
    UpdateAccessibleDefaultAction();
  }

 private:
  // views::Button:
  void OnEnabledChanged() override {
    RadioButton::OnEnabledChanged();
    UpdateAccessibleDefaultAction();
  }

  // OptionButtonBase:
  void OnSelectedChanged() override {
    RadioButton::OnSelectedChanged();
    // Override the default action verb updated in OptionButtonBase.
    UpdateAccessibleDefaultAction();
  }

  void UpdateAccessibleDefaultAction() {
    GetViewAccessibility().SetDefaultActionVerb(
        ax::mojom::DefaultActionVerb::kClick);
  }
};

BEGIN_METADATA(ComboboxMenuOption)
END_METADATA

class ComboboxMenuOptionGroup : public RadioButtonGroup {
  METADATA_HEADER(ComboboxMenuOptionGroup, RadioButtonGroup)

 public:
  ComboboxMenuOptionGroup()
      : RadioButtonGroup(kMaxMenuWidth,
                         kMenuBorderInsets,
                         0,
                         RadioButton::IconDirection::kLeading,
                         RadioButton::IconType::kCheck,
                         kMenuItemInnerPadding,
                         kCheckmarkLabelSpacing) {
    GetViewAccessibility().SetRole(ax::mojom::Role::kListBox);
    GetViewAccessibility().SetName(
        "", ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  }

  // RadioButtonGroup:
  RadioButton* AddButton(RadioButton::PressedCallback callback,
                         const std::u16string& label) override {
    auto* button = AddChildView(std::make_unique<ComboboxMenuOption>(
        group_width_ - inside_border_insets_.width(), std::move(callback),
        label));
    button->set_delegate(this);
    buttons_.push_back(button);
    return button;
  }
};

BEGIN_METADATA(ComboboxMenuOptionGroup)
END_METADATA

}  // namespace

//------------------------------------------------------------------------------
// Combobox::ComboboxMenuView:
// The contents of combobox drop down menu which contains a list of items
// corresponding to the items in combobox model. The selected item will show a
// leading checked icon.
class Combobox::ComboboxMenuView : public views::View {
  METADATA_HEADER(ComboboxMenuView, views::View)

 public:
  explicit ComboboxMenuView(base::WeakPtr<Combobox> combobox)
      : combobox_(combobox),
        background_shield_(this,
                           kMenuBackgroundColorId,
                           ColorProvider::kBackgroundBlurSigma,
                           kMenuRoundedCorners) {
    SetLayoutManager(std::make_unique<views::FillLayout>());

    scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
        views::ScrollView::ScrollWithLayers::kEnabled));
    scroll_view_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
    scroll_view_->layer()->SetFillsBoundsOpaquely(false);
    scroll_view_->ClipHeightTo(0, std::numeric_limits<int>::max());
    scroll_view_->SetDrawOverflowIndicator(false);
    scroll_view_->SetBackgroundColor(std::nullopt);
    scroll_view_->SetVerticalScrollBarMode(
        views::ScrollView::ScrollBarMode::kHiddenButEnabled);

    // Create a radio buttons group for item list.
    menu_item_group_ =
        scroll_view_->SetContents(std::make_unique<ComboboxMenuOptionGroup>());
    UpdateMenuContent();

    // Set border.
    SetBorder(std::make_unique<views::HighlightBorder>(
        kMenuRoundedCorners,
        views::HighlightBorder::Type::kHighlightBorderOnShadow));
  }
  ComboboxMenuView(const ComboboxMenuView&) = delete;
  ComboboxMenuView& operator=(const ComboboxMenuView&) = delete;
  ~ComboboxMenuView() override = default;

  void SelectItem(int index) { menu_item_group_->SelectButtonAtIndex(index); }

  OptionButtonBase* GetSelectedItemView() const {
    auto selected_views = menu_item_group_->GetSelectedButtons();
    if (selected_views.empty()) {
      return nullptr;
    }

    return selected_views[0];
  }

  void UpdateMenuContent() {
    menu_item_group_->RemoveAllChildViews();

    // Build a radio button group according to current combobox model.
    for (size_t i = 0; i < combobox_->model_->GetItemCount(); i++) {
      auto* item = menu_item_group_->AddButton(
          base::BindRepeating(&Combobox::MenuSelectionAt, combobox_, i),
          combobox_->model_->GetItemAt(i));
      item->SetLabelStyle(TypographyToken::kCrosButton2);
      item->SetLabelColorId(kMenuTextColorId);
      item->SetSelected(combobox_->selected_index_.value_or(-1) == i);
    }
    GetSelectedItemView()->ScrollViewToVisible();
  }

  void ScrollToSelectedView() {
    if (GetSelectedItemView()) {
      GetSelectedItemView()->ScrollViewToVisible();
    }
  }

  views::View* MenuItemAtIndex(int index) const {
    if (index >= 0 &&
        index < static_cast<int>(menu_item_group_->children().size())) {
      return menu_item_group_->children()[index];
    }
    return nullptr;
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    gfx::Size size = views::View::CalculatePreferredSize(available_size);
    size.SetToMin(gfx::Size(kMaxMenuWidth, kMaxMenuHeight));
    return size;
  }

 private:
  const base::WeakPtr<Combobox> combobox_;
  const BlurredBackgroundShield background_shield_;

  // Owned by this.
  raw_ptr<ComboboxMenuOptionGroup> menu_item_group_;
  raw_ptr<views::ScrollView> scroll_view_;
};

BEGIN_METADATA(Combobox, ComboboxMenuView)
END_METADATA

//------------------------------------------------------------------------------
// Combobox::ComboboxEventHandler:
// Handles the mouse and touch event that happens outside combobox and its drop
// down menu.
class Combobox::ComboboxEventHandler : public ui::EventHandler {
 public:
  explicit ComboboxEventHandler(Combobox* combobox) : combobox_(combobox) {
    aura::Env::GetInstance()->AddPreTargetHandler(
        this, ui::EventTarget::Priority::kSystem);
  }

  ComboboxEventHandler(const ComboboxEventHandler&) = delete;
  ComboboxEventHandler& operator=(const ComboboxEventHandler&) = delete;
  ~ComboboxEventHandler() override {
    aura::Env::GetInstance()->RemovePreTargetHandler(this);
  }

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override { OnLocatedEvent(event); }

  void OnTouchEvent(ui::TouchEvent* event) override { OnLocatedEvent(event); }

  void OnKeyEvent(ui::KeyEvent* event) override {
    // If the menu is shown, route the key event to the combobox view, to handle
    // keys impact the menu selection/state even if the combobox view is not
    // currently focused (which may be the case if the combobox is shown within
    // non-activatable widget, e.g. a system tray bubble).
    if (combobox_->IsMenuRunning() && !combobox_->HasFocus()) {
      combobox_->OnKeyEvent(event);
    }
  }

 private:
  void OnLocatedEvent(ui::LocatedEvent* event) {
    // Close drop down menu if certain mouse or touch events happening outside
    // combobox or menu area.
    if (!combobox_->IsMenuRunning()) {
      return;
    }

    // Get event location in screen.
    gfx::Point event_location = event->location();
    aura::Window* event_target = static_cast<aura::Window*>(event->target());
    wm::ConvertPointToScreen(event_target, &event_location);

    const bool event_in_combobox =
        combobox_->GetBoundsInScreen().Contains(event_location);
    const bool event_in_menu =
        combobox_->menu_->GetWindowBoundsInScreen().Contains(event_location);
    switch (event->type()) {
      case ui::EventType::kMousewheel:
        // Close menu if scrolling outside menu.
        if (!event_in_menu) {
          combobox_->CloseDropDownMenu();
        }
        break;
      case ui::EventType::kMousePressed:
      case ui::EventType::kTouchPressed:
        // Close menu if pressing outside menu and combobox.
        if (!event_in_menu && !event_in_combobox) {
          event->StopPropagation();
          combobox_->CloseDropDownMenu();
        }
        break;
      default:
        break;
    }
  }

  const raw_ptr<Combobox, DanglingUntriaged> combobox_;
};

//------------------------------------------------------------------------------
// Combobox:
Combobox::Combobox(std::unique_ptr<ui::ComboboxModel> model)
    : Combobox(model.get()) {
  owned_model_ = std::move(model);
}

Combobox::Combobox(ui::ComboboxModel* model)
    : views::Button(base::BindRepeating(&Combobox::OnComboboxPressed,
                                        base::Unretained(this))),
      model_(model),
      title_(AddChildView(std::make_unique<views::Label>())),
      drop_down_arrow_(AddChildView(std::make_unique<views::ImageView>(
          ui::ImageModel::FromVectorIcon(kDropDownArrowIcon,
                                         kInactiveTitleAndIconColorId,
                                         kArrowIconSize)))) {
  // Initialize the combobox with given model.
  CHECK(model_);
  observation_.Observe(model_.get());
  SetSelectedIndex(model_->GetDefaultIndex());
  OnPerformAction();
  OnComboboxModelChanged(model_);

  // Set up layout.
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetInteriorMargin(kComboboxBorderInsets);
  // TODO(crbug.com/40232718): See View::SetLayoutManagerUseConstrainedSpace.
  SetLayoutManagerUseConstrainedSpace(false);

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
  title_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // Set up the ink drop.
  StyleUtil::InstallRoundedCornerHighlightPathGenerator(
      this, kComboboxRoundedCorners);
  StyleUtil::SetUpInkDropForButton(this);
  views::FocusRing::Get(this)->SetProperty(views::kViewIgnoredByLayoutKey,
                                           /*ignored=*/true);

  event_handler_ = std::make_unique<ComboboxEventHandler>(this);

  // `ax::mojom::Role::kComboBox` is for UI elements with a dropdown and
  // an editable text field, which `views::Combobox` does not have. Use
  // `ax::mojom::Role::kPopUpButton` to match an HTML <select> element.
  GetViewAccessibility().SetRole(ax::mojom::Role::kPopUpButton);
  UpdateExpandedCollapsedAccessibleState();
  UpdateAccessibleDefaultAction();
}

Combobox::~Combobox() = default;

void Combobox::SetSelectionChangedCallback(base::RepeatingClosure callback) {
  callback_ = std::move(callback);
}

void Combobox::SetSelectedIndex(std::optional<size_t> index) {
  if (selected_index_ == index) {
    return;
  }

  if (index.has_value()) {
    CHECK_LT(index.value(), model_->GetItemCount());
  }

  selected_index_ = index;

  if (!selected_index_.has_value()) {
    return;
  }

  // Update selected item on menu if the menu is opening.
  if (menu_view_) {
    menu_view_->SelectItem(selected_index_.value());
    UpdateAccessibleAccessibleActiveDescendantId();
  }
}

bool Combobox::SelectValue(const std::u16string& value) {
  for (size_t i = 0; i < model_->GetItemCount(); ++i) {
    if (value == model_->GetItemAt(i)) {
      SetSelectedIndex(i);
      return true;
    }
  }
  return false;
}

bool Combobox::IsMenuRunning() const {
  return !!menu_;
}

gfx::Size Combobox::GetMenuViewSize() const {
  if (!menu_) {
    return gfx::Size();
  }
  return menu_view_->size();
}

views::View* Combobox::MenuItemAtIndex(int index) const {
  if (!menu_) {
    return nullptr;
  }
  return menu_view_->MenuItemAtIndex(index);
}

views::View* Combobox::MenuView() const {
  return menu_view_;
}

void Combobox::SetCallback(PressedCallback callback) {
  NOTREACHED() << "Clients shouldn't modify this. Maybe you want to use "
                  "SetSelectionChangedCallback?";
}

void Combobox::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // Move menu with combobox accordingly.
  if (menu_) {
    menu_->SetBounds(GetExpectedMenuBounds());
  }
}

void Combobox::OnBlur() {
  if (menu_) {
    CloseDropDownMenu();
  }

  views::Button::OnBlur();
}

void Combobox::AddedToWidget() {
  widget_observer_.Observe(GetWidget());
}

void Combobox::RemovedFromWidget() {
  widget_observer_.Reset();
}

void Combobox::Layout(PassKey) {
  LayoutSuperclass<views::Button>(this);
  views::FocusRing::Get(this)->DeprecatedLayoutImmediately();
}

void Combobox::OnWidgetBoundsChanged(views::Widget* widget,
                                     const gfx::Rect& bounds) {
  if (menu_) {
    menu_->SetBounds(GetExpectedMenuBounds());
  }
}

std::u16string Combobox::GetTextForRow(size_t row) const {
  return model_->IsItemSeparatorAt(row) ? std::u16string()
                                        : model_->GetItemAt(row);
}

void Combobox::SelectMenuItemForTest(size_t row) {
  MenuSelectionAt(row);
}

gfx::Rect Combobox::GetExpectedMenuBounds() const {
  CHECK(menu_view_);
  WorkAreaInsets* work_area =
      WorkAreaInsets::ForWindow(GetWidget()->GetNativeWindow());
  const gfx::Rect available_bounds = work_area->user_work_area_bounds();

  const gfx::Size preferred_size = menu_view_->GetPreferredSize();
  const gfx::Rect combobox_bounds = GetBoundsInScreen();

  // Decide whether to show the combobox menu below (default) or above the
  // combobox:
  // if the combobox menu fits below the combobox, show it below.
  const int height_below =
      available_bounds.bottom() - combobox_bounds.bottom() - kMenuOffset.y();
  bool show_below_combobox = height_below >= preferred_size.height();
  // If the combobox menu does not fit below combobox, show it above the
  // combobox of there is more space available above.
  if (!show_below_combobox) {
    const int height_above =
        combobox_bounds.y() - available_bounds.y() - kMenuOffset.y();
    show_below_combobox = height_below >= height_above;
  }

  gfx::Rect preferred_bounds =
      show_below_combobox
          ? gfx::Rect(combobox_bounds.bottom_left() + kMenuOffset,
                      preferred_size)
          : gfx::Rect(
                combobox_bounds.origin() +
                    gfx::Vector2d(kMenuOffset.x(),
                                  -preferred_size.height() - kMenuOffset.y()),
                preferred_size);

  // If the combobox view is offscreen, translate the preferred combobox bounds
  // to fit available bounds.
  if (show_below_combobox && combobox_bounds.bottom() < available_bounds.y()) {
    preferred_bounds.Offset(0, available_bounds.y() - combobox_bounds.bottom());
  } else if (!show_below_combobox &&
             combobox_bounds.y() > available_bounds.bottom()) {
    preferred_bounds.Offset(0, available_bounds.bottom() - combobox_bounds.y());
  }

  preferred_bounds.Intersect(available_bounds);
  return preferred_bounds;
}

void Combobox::MenuSelectionAt(size_t index) {
  SetSelectedIndex(index);
  // Close the menu once a selection is made.
  CloseDropDownMenu();
}

void Combobox::OnComboboxPressed() {
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

void Combobox::ShowDropDownMenu() {
  auto* widget = GetWidget();
  if (!widget) {
    return;
  }

  auto menu_view =
      std::make_unique<ComboboxMenuView>(weak_ptr_factory_.GetWeakPtr());
  menu_view_ = menu_view.get();

  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.shadow_elevation = kMenuShadowElevation;
  params.corner_radius = kMenuRoundedCorners.lower_left();

  aura::Window* root_window = widget->GetNativeWindow()->GetRootWindow();
  params.parent = root_window->GetChildById(kShellWindowId_MenuContainer);
  params.bounds = GetExpectedMenuBounds();

  menu_ = std::make_unique<views::Widget>(std::move(params));
  menu_->SetContentsView(std::move(menu_view));
  menu_->Show();
  menu_view_->ScrollToSelectedView();
  UpdateExpandedCollapsedAccessibleState();
  UpdateAccessibleAccessibleActiveDescendantId();

  SetBackground(views::CreateThemedRoundedRectBackground(
      kComboboxActiveColorId, kComboboxRoundedCorners));
  title_->SetEnabledColorId(kActiveTitleAndIconColorId);
  drop_down_arrow_->SetImage(ui::ImageModel::FromVectorIcon(
      kDropDownArrowIcon, kActiveTitleAndIconColorId, kArrowIconSize));

  RequestFocus();
}

void Combobox::CloseDropDownMenu() {
  menu_view_ = nullptr;
  menu_.reset();
  UpdateExpandedCollapsedAccessibleState();
  UpdateAccessibleAccessibleActiveDescendantId();

  closed_time_ = base::TimeTicks::Now();
  SetBackground(nullptr);
  title_->SetEnabledColorId(kInactiveTitleAndIconColorId);
  drop_down_arrow_->SetImage(ui::ImageModel::FromVectorIcon(
      kDropDownArrowIcon, kInactiveTitleAndIconColorId, kArrowIconSize));

  // Commit the selection once the combobox view state has been updated.
  // NOTE: This may run selection callback, which may end up deleting this,
  // depending on how the callback is handled.
  OnPerformAction();
}

void Combobox::OnPerformAction() {
  if (selected_index_ == last_commit_selection_) {
    return;
  }

  last_commit_selection_ = selected_index_;

  if (selected_index_.has_value()) {
    title_->SetText(model_->GetItemAt(selected_index_.value()));
  } else {
    title_->SetText(std::u16string());
  }

  if (selected_index_) {
    GetViewAccessibility().SetPosInSet(
        base::checked_cast<int>(selected_index_.value()));
    GetViewAccessibility().SetSetSize(
        base::checked_cast<int>(model_->GetItemCount()));
  } else {
    GetViewAccessibility().ClearPosInSet();
    GetViewAccessibility().ClearSetSize();
  }

  GetViewAccessibility().SetValue(title_->GetText());

  if (selected_index_.has_value() && callback_) {
    callback_.Run();
  }
}

void Combobox::OnComboboxModelChanged(ui::ComboboxModel* model) {
  DCHECK_EQ(model_, model);

  // If the selection is no longer valid (or the model is empty), restore the
  // default index.
  if (selected_index_ >= model_->GetItemCount() ||
      model_->GetItemCount() == 0 ||
      model_->IsItemSeparatorAt(selected_index_.value())) {
    SetSelectedIndex(model_->GetDefaultIndex());
  }

  if (menu_view_) {
    menu_view_->UpdateMenuContent();
    UpdateAccessibleAccessibleActiveDescendantId();
  }
}

void Combobox::OnComboboxModelDestroying(ui::ComboboxModel* model) {
  // Reset selected index to avoid using the destroying model.
  SetSelectedIndex(std::nullopt);
  model_ = nullptr;
  observation_.Reset();
  CloseDropDownMenu();
}

bool Combobox::SkipDefaultKeyEventProcessing(const ui::KeyEvent& e) {
  if (!IsMenuRunning()) {
    return false;
  }

  // Let combobox directly handle keys that update combobox menu selection if
  // the menu is running.
  if (e.key_code() == ui::VKEY_DOWN || e.key_code() == ui::VKEY_END ||
      e.key_code() == ui::VKEY_NEXT || e.key_code() == ui::VKEY_HOME ||
      e.key_code() == ui::VKEY_PRIOR || e.key_code() == ui::VKEY_UP ||
      e.key_code() == ui::VKEY_TAB) {
    return true;
  }

  // Escape should close the drop down list when it is active, not host UI.
  if (e.key_code() == ui::VKEY_ESCAPE && !e.IsShiftDown() &&
      !e.IsControlDown() && !e.IsAltDown() && !e.IsAltGrDown()) {
    return true;
  }

  return false;
}

bool Combobox::OnKeyPressed(const ui::KeyEvent& e) {
  CHECK_EQ(e.type(), ui::EventType::kKeyPressed);

  CHECK(selected_index_.has_value());
  CHECK_LT(selected_index_.value(), model_->GetItemCount());

  const auto index_at_or_after = [](ui::ComboboxModel* model,
                                    size_t index) -> std::optional<size_t> {
    for (; index < model->GetItemCount(); ++index) {
      if (!model->IsItemSeparatorAt(index) && model->IsItemEnabledAt(index)) {
        return index;
      }
    }
    return std::nullopt;
  };

  const auto index_before = [](ui::ComboboxModel* model,
                               size_t index) -> std::optional<size_t> {
    for (; index > 0; --index) {
      const auto prev = index - 1;
      if (!model->IsItemSeparatorAt(prev) && model->IsItemEnabledAt(prev)) {
        return prev;
      }
    }
    return std::nullopt;
  };

  std::optional<size_t> new_index;
  switch (e.key_code()) {
    // Show the menu on F4 without modifiers.
    case ui::VKEY_F4:
      if (e.IsAltDown() || e.IsAltGrDown() || e.IsControlDown()) {
        return false;
      }
      ShowDropDownMenu();
      return true;

    // Move to the next item if any, or show the menu on Alt+Down like Windows.
    case ui::VKEY_DOWN:
      if (e.IsAltDown()) {
        ShowDropDownMenu();
        return true;
      }
      new_index = index_at_or_after(model_, selected_index_.value() + 1);
      break;

    // Move to the end of the list.
    case ui::VKEY_END:
    case ui::VKEY_NEXT:  // Page down.
      new_index = index_before(model_, model_->GetItemCount());
      break;

    // Move to the beginning of the list.
    case ui::VKEY_HOME:
    case ui::VKEY_PRIOR:  // Page up.
      new_index = index_at_or_after(model_, 0);
      break;

    // Move to the previous item if any.
    case ui::VKEY_UP:
      new_index = index_before(model_, selected_index_.value());
      break;
    case ui::VKEY_TAB:
      if (menu_view_) {
        new_index =
            e.IsShiftDown()
                ? index_before(model_, selected_index_.value())
                : index_at_or_after(model_, selected_index_.value() + 1);
        break;
      }
      // If menu is closed, proceed with the default TAB key behavior (and let
      // it move the focus away from the combobox).
      return views::Button::OnKeyPressed(e);
    case ui::VKEY_ESCAPE:
      if (menu_view_) {
        SetSelectedIndex(last_commit_selection_);
        CloseDropDownMenu();
        return true;
      }
      return views::Button::OnKeyPressed(e);
    default:
      return views::Button::OnKeyPressed(e);
  }

  // If menu is running, only update selected item on menu instead of committing
  // the selection. Otherwise, make the selection.
  if (new_index.has_value()) {
    SetSelectedIndex(new_index);
    if (!IsMenuRunning()) {
      OnPerformAction();
    }
  }
  return true;
}

void Combobox::OnEnabledChanged() {
  views::Button::OnEnabledChanged();
  UpdateAccessibleDefaultAction();
}

void Combobox::UpdateExpandedCollapsedAccessibleState() const {
  if (IsMenuRunning()) {
    GetViewAccessibility().SetIsExpanded();
  } else {
    GetViewAccessibility().SetIsCollapsed();
  }
}

void Combobox::UpdateAccessibleAccessibleActiveDescendantId() {
  OptionButtonBase* selected_button =
      menu_view_ ? menu_view_->GetSelectedItemView() : nullptr;
  if (selected_button) {
    GetViewAccessibility().SetActiveDescendant(*selected_button);
  } else {
    GetViewAccessibility().ClearActiveDescendant();
  }
}

void Combobox::UpdateAccessibleDefaultAction() {
  GetViewAccessibility().SetDefaultActionVerb(
      ax::mojom::DefaultActionVerb::kOpen);
}

BEGIN_METADATA(Combobox)
END_METADATA

}  // namespace ash
