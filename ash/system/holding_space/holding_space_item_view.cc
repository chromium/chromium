// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_view.h"

#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_item_view_delegate.h"
#include "ash/system/holding_space/holding_space_util.h"
#include "base/bind.h"
#include "ui/base/class_property.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/painter.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/vector_icons.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// A UI class property used to identify if a view is an instance of
// `HoldingSpaceItemView`. Class name is not an adequate identifier as it may be
// overridden by subclasses.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsHoldingSpaceItemViewProperty, false)

// Appearance.
constexpr size_t kCheckmarkBackgroundSize = 18;

// Helpers ---------------------------------------------------------------------

// Schedules repaint of `layer`.
void InvalidateLayer(ui::Layer* layer) {
  layer->SchedulePaint(gfx::Rect(layer->size()));
}

// CallbackPainter -------------------------------------------------------------

// A painter which delegates painting to a callback.
class CallbackPainter : public views::Painter {
 public:
  using Callback = base::RepeatingCallback<void(gfx::Canvas*, gfx::Size)>;

  CallbackPainter(const CallbackPainter&) = delete;
  CallbackPainter& operator=(const CallbackPainter&) = delete;
  ~CallbackPainter() override = default;

  // Creates a painted layer which delegates painting to `callback`.
  static std::unique_ptr<ui::LayerOwner> CreatePaintedLayer(Callback callback) {
    auto owner = views::Painter::CreatePaintedLayer(
        base::WrapUnique(new CallbackPainter(callback)));
    owner->layer()->SetFillsBoundsOpaquely(false);
    return owner;
  }

 private:
  explicit CallbackPainter(Callback callback) : callback_(callback) {}

  // views::Painter:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override {
    callback_.Run(canvas, size);
  }

  Callback callback_;
};

}  // namespace

// HoldingSpaceItemView --------------------------------------------------------

HoldingSpaceItemView::HoldingSpaceItemView(
    HoldingSpaceItemViewDelegate* delegate,
    const HoldingSpaceItem* item)
    : delegate_(delegate), item_(item), item_id_(item->id()) {
  // Subscribe to be notified of `item_` deletion. Note that it is safe to use a
  // raw pointer here since `this` owns the callback.
  item_deletion_subscription_ = item_->AddDeletionCallback(base::BindRepeating(
      [](HoldingSpaceItemView* view) { view->item_ = nullptr; },
      base::Unretained(this)));

  model_observer_.Observe(HoldingSpaceController::Get()->model());

  SetProperty(kIsHoldingSpaceItemViewProperty, true);

  set_context_menu_controller(delegate_);
  set_drag_controller(delegate_);

  SetNotifyEnterExitOnChild(true);

  // Accessibility.
  GetViewAccessibility().OverrideName(item->text());
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kListItem);

  // Layer.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // Focus.
  SetFocusBehavior(FocusBehavior::ALWAYS);
  focused_layer_owner_ =
      CallbackPainter::CreatePaintedLayer(base::BindRepeating(
          &HoldingSpaceItemView::OnPaintFocus, base::Unretained(this)));
  layer()->Add(focused_layer_owner_->layer());

  // Selection.
  selected_layer_owner_ =
      CallbackPainter::CreatePaintedLayer(base::BindRepeating(
          &HoldingSpaceItemView::OnPaintSelect, base::Unretained(this)));
  layer()->Add(selected_layer_owner_->layer());

  // This view's `selected_` state is represented differently depending on
  // `delegate_`'s selection UI. Register to be notified of changes.
  selection_ui_changed_subscription_ =
      delegate_->AddSelectionUiChangedCallback(base::BindRepeating(
          &HoldingSpaceItemView::OnSelectionUiChanged, base::Unretained(this)));

  delegate_->OnHoldingSpaceItemViewCreated(this);
}

HoldingSpaceItemView::~HoldingSpaceItemView() {
  if (delegate_)
    delegate_->OnHoldingSpaceItemViewDestroying(this);
}

// static
HoldingSpaceItemView* HoldingSpaceItemView::Cast(views::View* view) {
  return const_cast<HoldingSpaceItemView*>(
      Cast(const_cast<const views::View*>(view)));
}

// static
const HoldingSpaceItemView* HoldingSpaceItemView::Cast(
    const views::View* view) {
  DCHECK(HoldingSpaceItemView::IsInstance(view));
  return static_cast<const HoldingSpaceItemView*>(view);
}

// static
bool HoldingSpaceItemView::IsInstance(const views::View* view) {
  return view->GetProperty(kIsHoldingSpaceItemViewProperty);
}

void HoldingSpaceItemView::Reset() {
  delegate_ = nullptr;
}

bool HoldingSpaceItemView::HandleAccessibleAction(
    const ui::AXActionData& action_data) {
  return (delegate_ && delegate_->OnHoldingSpaceItemViewAccessibleAction(
                           this, action_data)) ||
         views::View::HandleAccessibleAction(action_data);
}

void HoldingSpaceItemView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  gfx::Rect bounds = GetLocalBounds();

  // Selection ring.
  selected_layer_owner_->layer()->SetBounds(bounds);
  InvalidateLayer(selected_layer_owner_->layer());

  // Focus ring.
  // NOTE: The focus ring is painted just outside the bounds for this view.
  bounds.Inset(gfx::Insets(kHoldingSpaceFocusInsets));
  focused_layer_owner_->layer()->SetBounds(bounds);
  InvalidateLayer(focused_layer_owner_->layer());
}

void HoldingSpaceItemView::OnFocus() {
  InvalidateLayer(focused_layer_owner_->layer());
}

void HoldingSpaceItemView::OnBlur() {
  InvalidateLayer(focused_layer_owner_->layer());
}

void HoldingSpaceItemView::OnGestureEvent(ui::GestureEvent* event) {
  if (delegate_ && delegate_->OnHoldingSpaceItemViewGestureEvent(this, *event))
    event->SetHandled();
}

bool HoldingSpaceItemView::OnKeyPressed(const ui::KeyEvent& event) {
  return delegate_ && delegate_->OnHoldingSpaceItemViewKeyPressed(this, event);
}

void HoldingSpaceItemView::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
      UpdatePin();
      break;
    default:
      break;
  }
  views::View::OnMouseEvent(event);
}

bool HoldingSpaceItemView::OnMousePressed(const ui::MouseEvent& event) {
  return delegate_ &&
         delegate_->OnHoldingSpaceItemViewMousePressed(this, event);
}

void HoldingSpaceItemView::OnMouseReleased(const ui::MouseEvent& event) {
  if (delegate_)
    delegate_->OnHoldingSpaceItemViewMouseReleased(this, event);
}

void HoldingSpaceItemView::OnThemeChanged() {
  views::View::OnThemeChanged();
  AshColorProvider* const ash_color_provider = AshColorProvider::Get();

  // Background.
  SetBackground(views::CreateRoundedRectBackground(
      ash_color_provider->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive),
      kHoldingSpaceCornerRadius));

  // Checkmark.
  checkmark_->SetBackground(holding_space_util::CreateCircleBackground(
      ash_color_provider->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kFocusRingColor),
      kCheckmarkBackgroundSize));
  checkmark_->SetImage(gfx::CreateVectorIcon(
      kCheckIcon, kHoldingSpaceIconSize,
      ash_color_provider->IsDarkModeEnabled() ? gfx::kGoogleGrey900
                                              : SK_ColorWHITE));

  // Focused/selected layers.
  InvalidateLayer(focused_layer_owner_->layer());
  InvalidateLayer(selected_layer_owner_->layer());

  if (!pin_)
    return;

  // Pin.
  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);
  const gfx::ImageSkia unpinned_icon = gfx::CreateVectorIcon(
      views::kUnpinIcon, kHoldingSpaceIconSize, icon_color);
  const gfx::ImageSkia pinned_icon =
      gfx::CreateVectorIcon(views::kPinIcon, kHoldingSpaceIconSize, icon_color);
  pin_->SetImage(views::Button::STATE_NORMAL, unpinned_icon);
  pin_->SetToggledImage(views::Button::STATE_NORMAL, &pinned_icon);
}

void HoldingSpaceItemView::OnHoldingSpaceItemUpdated(
    const HoldingSpaceItem* item) {
  if (item_ == item)
    GetViewAccessibility().OverrideName(item->text());
}

void HoldingSpaceItemView::StartDrag(const ui::LocatedEvent& event,
                                     ui::mojom::DragEventSource source) {
  int drag_operations = GetDragOperations(event.location());
  if (drag_operations == ui::DragDropTypes::DRAG_NONE)
    return;

  views::Widget* widget = GetWidget();
  DCHECK(widget);

  if (widget->dragged_view())
    return;

  auto data = std::make_unique<ui::OSExchangeData>();
  WriteDragData(event.location(), data.get());

  gfx::Point widget_location(event.location());
  views::View::ConvertPointToWidget(this, &widget_location);
  widget->RunShellDrag(this, std::move(data), widget_location, drag_operations,
                       source);
}

void HoldingSpaceItemView::SetSelected(bool selected) {
  if (selected_ == selected)
    return;

  selected_ = selected;
  InvalidateLayer(selected_layer_owner_->layer());

  if (delegate_)
    delegate_->OnHoldingSpaceItemViewSelectedChanged(this);

  OnSelectionUiChanged();
}

views::ImageView* HoldingSpaceItemView::AddCheckmark(views::View* parent) {
  DCHECK(!checkmark_);
  checkmark_ = parent->AddChildView(std::make_unique<views::ImageView>());
  checkmark_->SetID(kHoldingSpaceItemCheckmarkId);
  checkmark_->SetVisible(selected());
  return checkmark_;
}

views::ToggleImageButton* HoldingSpaceItemView::AddPin(views::View* parent) {
  DCHECK(!pin_);

  pin_ = parent->AddChildView(std::make_unique<views::ToggleImageButton>());
  pin_->SetID(kHoldingSpaceItemPinButtonId);
  pin_->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  pin_->SetImageHorizontalAlignment(
      views::ToggleImageButton::HorizontalAlignment::ALIGN_CENTER);
  pin_->SetImageVerticalAlignment(
      views::ToggleImageButton::VerticalAlignment::ALIGN_MIDDLE);
  pin_->SetVisible(false);

  pin_->SetCallback(base::BindRepeating(&HoldingSpaceItemView::OnPinPressed,
                                        base::Unretained(this)));

  return pin_;
}

void HoldingSpaceItemView::OnSelectionUiChanged() {
  const bool multiselect =
      delegate_ && delegate_->selection_ui() ==
                       HoldingSpaceItemViewDelegate::SelectionUi::kMultiSelect;

  checkmark_->SetVisible(selected() && multiselect);
}

void HoldingSpaceItemView::OnPaintFocus(gfx::Canvas* canvas, gfx::Size size) {
  if (!HasFocus())
    return;

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor));
  flags.setStrokeWidth(views::PlatformStyle::kFocusHaloThickness);
  flags.setStyle(cc::PaintFlags::kStroke_Style);

  gfx::Rect bounds = gfx::Rect(size);
  bounds.Inset(gfx::Insets(flags.getStrokeWidth() / 2));
  canvas->DrawRoundRect(bounds, kHoldingSpaceFocusCornerRadius, flags);
}

void HoldingSpaceItemView::OnPaintSelect(gfx::Canvas* canvas, gfx::Size size) {
  if (!selected_)
    return;

  const SkColor color =
      SkColorSetA(AshColorProvider::Get()->GetControlsLayerColor(
                      AshColorProvider::ControlsLayerType::kFocusRingColor),
                  kHoldingSpaceSelectedOverlayOpacity * 0xFF);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(color);

  canvas->DrawRoundRect(gfx::Rect(size), kHoldingSpaceCornerRadius, flags);
}

void HoldingSpaceItemView::OnPinPressed() {
  // If the associated `item()` has been deleted then `this` is in the process
  // of being destroyed and no action needs to be taken.
  if (!item())
    return;

  const bool is_item_pinned =
      HoldingSpaceController::Get()->model()->ContainsItem(
          HoldingSpaceItem::Type::kPinnedFile, item()->file_path());

  // Unpinning `item()` may result in the destruction of this view.
  auto weak_ptr = weak_factory_.GetWeakPtr();
  if (is_item_pinned)
    HoldingSpaceController::Get()->client()->UnpinItems({item()});
  else
    HoldingSpaceController::Get()->client()->PinItems({item()});

  if (weak_ptr)
    UpdatePin();
}

void HoldingSpaceItemView::UpdatePin() {
  // If the associated `item()` has been deleted then `this` is in the process
  // of being destroyed and no action needs to be taken.
  if (!item())
    return;

  if (!IsMouseHovered()) {
    pin_->SetVisible(false);
    OnPinVisibilityChanged(false);
    return;
  }

  const bool is_item_pinned =
      HoldingSpaceController::Get()->model()->ContainsItem(
          HoldingSpaceItem::Type::kPinnedFile, item()->file_path());

  pin_->SetToggled(!is_item_pinned);
  pin_->SetVisible(true);
  OnPinVisibilityChanged(true);
}

BEGIN_METADATA(HoldingSpaceItemView, views::View)
END_METADATA

}  // namespace ash
