// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/combobox.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/radio_button.h"
#include "ash/style/radio_button_group.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "base/functional/bind.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
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
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The color constants.
constexpr ui::ColorId kTextAndIconColorId = cros_tokens::kCrosSysOnSurface;
constexpr ui::ColorId kMenuBackgroudnColorId =
    cros_tokens::kCrosSysSystemBaseElevated;
constexpr ui::ColorId kComboboxActiveColorId =
    cros_tokens::kCrosSysSystemPrimaryContainer;

// The layout parameters.
constexpr gfx::RoundedCornersF kComboboxRoundedCorners =
    gfx::RoundedCornersF(12, 12, 12, 4);
constexpr gfx::RoundedCornersF kMenuRoundedCorners =
    gfx::RoundedCornersF(4, 12, 12, 12);
constexpr gfx::Insets kComboboxBorderInsets = gfx::Insets::TLBR(4, 10, 4, 4);
constexpr gfx::Insets kMenuBorderInsets = gfx::Insets::TLBR(16, 0, 12, 0);
constexpr gfx::Insets kMenuItemInnerPadding = gfx::Insets::VH(8, 16);
constexpr int kArrowIconSize = 20;
constexpr int kCheckmarkLabelSpacing = 16;
constexpr int kMinMenuWidth = 256;
constexpr gfx::Vector2d kMenuOffset(0, 8);
constexpr int kMenuShadowElevation = 12;

}  // namespace

//------------------------------------------------------------------------------
// Combobox::ComboboxMenuView:
// The contents of combobox drop down menu which contains a list of items
// corresponding to the items in combobox model. The selected item will show a
// leading checked icon.
class Combobox::ComboboxMenuView : public views::View {
 public:
  METADATA_HEADER(ComboboxMenuView);
  explicit ComboboxMenuView(Combobox* combobox) : combobox_(combobox) {
    SetLayoutManager(std::make_unique<views::FillLayout>());

    // Create a radio buttons group for item list.
    menu_item_group_ = AddChildView(std::make_unique<RadioButtonGroup>(
        kMinMenuWidth, kMenuBorderInsets, 0,
        RadioButton::IconDirection::kLeading, RadioButton::IconType::kCheck,
        kMenuItemInnerPadding, kCheckmarkLabelSpacing));
    UpdateMenuContent();

    // Set background and border.
    SetBackground(views::CreateThemedRoundedRectBackground(
        kMenuBackgroudnColorId, kMenuRoundedCorners,
        /*for_border_thickness=*/0));
    SetBorder(std::make_unique<views::HighlightBorder>(
        kMenuRoundedCorners,
        views::HighlightBorder::Type::kHighlightBorderOnShadow));
  }
  ComboboxMenuView(const ComboboxMenuView&) = delete;
  ComboboxMenuView& operator=(const ComboboxMenuView&) = delete;
  ~ComboboxMenuView() override = default;

  void SelectItem(int index) { menu_item_group_->SelectButtonAtIndex(index); }

  void UpdateMenuContent() {
    menu_item_group_->RemoveAllChildViews();

    // Build a radio button group according to current combobox model.
    for (size_t i = 0; i < combobox_->model_->GetItemCount(); i++) {
      auto* item = menu_item_group_->AddButton(
          base::BindRepeating(&Combobox::MenuSelectionAt,
                              base::Unretained(combobox_), i),
          combobox_->model_->GetDropDownTextAt(i));
      item->SetLabelStyle(TypographyToken::kCrosButton2);
      item->SetLabelColorId(kTextAndIconColorId);
      item->SetSelected(combobox_->selected_index_.value() == i);
    }
  }

 private:
  const raw_ptr<Combobox> combobox_;

  // Owned by this.
  raw_ptr<RadioButtonGroup> menu_item_group_;
};

BEGIN_METADATA(Combobox, ComboboxMenuView, views::View)
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

 private:
  void OnLocatedEvent(ui::LocatedEvent* event) {
    // If there is a mouse or touch event happening outside the combobox and
    // drop down menu, the drop down menu should be closed.
    if (event->type() != ui::ET_MOUSE_PRESSED &&
        event->type() != ui::ET_TOUCH_PRESSED) {
      return;
    }

    if (!combobox_->IsMenuRunning()) {
      return;
    }

    gfx::Point event_location = event->location();
    aura::Window* event_target = static_cast<aura::Window*>(event->target());
    wm::ConvertPointToScreen(event_target, &event_location);
    if (!combobox_->menu_->GetWindowBoundsInScreen().Contains(event_location) &&
        !combobox_->GetBoundsInScreen().Contains(event_location)) {
      combobox_->CloseDropDownMenu();
    }
  }

  const raw_ptr<Combobox> combobox_;
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
      title_(AddChildView(std::make_unique<views::Label>())) {
  // Initialize the combobox with given model.
  CHECK(model_);
  observation_.Observe(model_.get());
  SetSelectedIndex(model_->GetDefaultIndex());
  OnComboboxModelChanged(model_);

  // Set up layout.
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      /*orientation=*/views::BoxLayout::Orientation::kHorizontal,
      /*inside_border_insets=*/kComboboxBorderInsets));

  // Stylize the title.
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosTitle1,
                                        *title_.get());
  title_->SetEnabledColorId(kTextAndIconColorId);

  // Add the following drop down arrow icon.
  AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          kDropDownArrowIcon, kTextAndIconColorId, kArrowIconSize)));

  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // Set up the ink drop.
  StyleUtil::InstallRoundedCornerHighlightPathGenerator(
      this, kComboboxRoundedCorners);
  StyleUtil::SetUpInkDropForButton(this);

  event_handler_ = std::make_unique<ComboboxEventHandler>(this);
}

Combobox::~Combobox() = default;

void Combobox::SetSelectionChangedCallback(base::RepeatingClosure callback) {
  callback_ = std::move(callback);
}

void Combobox::SetSelectedIndex(absl::optional<size_t> index) {
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
  }

  OnPerformAction();
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

gfx::Rect Combobox::GetExpectedMenuBounds() const {
  CHECK(menu_view_);
  return gfx::Rect(GetBoundsInScreen().bottom_left() + kMenuOffset,
                   menu_view_->GetPreferredSize());
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

  if ((base::TimeTicks::Now() - closed_time_) >
      views::kMinimumTimeBetweenButtonClicks) {
    ShowDropDownMenu();
  }
}

void Combobox::ShowDropDownMenu() {
  auto* widget = GetWidget();
  if (!widget) {
    return;
  }

  auto menu_view = std::make_unique<ComboboxMenuView>(this);
  menu_view_ = menu_view.get();

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
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

  SetBackground(views::CreateThemedRoundedRectBackground(
      kComboboxActiveColorId, kComboboxRoundedCorners,
      /*for_border_thickness=*/0));
}

void Combobox::CloseDropDownMenu() {
  menu_view_ = nullptr;
  menu_.reset();
  closed_time_ = base::TimeTicks::Now();
  SetBackground(nullptr);
}

void Combobox::OnPerformAction() {
  CHECK(selected_index_.has_value());
  title_->SetText(model_->GetItemAt(selected_index_.value()));

  SchedulePaint();

  if (callback_) {
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
  }
}

void Combobox::OnComboboxModelDestroying(ui::ComboboxModel* model) {
  CloseDropDownMenu();
  model_ = nullptr;
  observation_.Reset();
}

BEGIN_METADATA(Combobox, views::Button)
END_METADATA

}  // namespace ash
