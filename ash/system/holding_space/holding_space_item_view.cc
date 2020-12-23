// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_view.h"

#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_item_view_delegate.h"
#include "base/bind.h"
#include "ui/base/class_property.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/metadata/metadata_impl_macros.h"
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
  model_observer_.Observe(HoldingSpaceController::Get()->model());

  SetProperty(kIsHoldingSpaceItemViewProperty, true);

  set_context_menu_controller(delegate_);
  set_drag_controller(delegate_);

  SetNotifyEnterExitOnChild(true);

  // Accessibility.
  GetViewAccessibility().OverrideName(item->text());
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kButton);

  // Background.
  SetBackground(views::CreateRoundedRectBackground(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive),
      kHoldingSpaceCornerRadius));

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

  // Ink drop.
  // Note that `ink_drop_container_` is added to the view hierarchy to parent
  // any created ink drop layers. This will allow ink drop layers to animate
  // in/out with the layer for this view as well as fix a crash in which ink
  // drop layers were attempted to be reordered during destruction of this view.
  ink_drop_container_ =
      AddChildView(std::make_unique<views::InkDropContainerView>());
  SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);
  SetInkDropVisibleOpacity(
      AshColorProvider::Get()->GetRippleAttributes().inkdrop_opacity);

  // Ink drop layers should match the corner radius of this view.
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kHoldingSpaceCornerRadius);

  delegate_->OnHoldingSpaceItemViewCreated(this);
}

HoldingSpaceItemView::~HoldingSpaceItemView() = default;

// static
HoldingSpaceItemView* HoldingSpaceItemView::Cast(views::View* view) {
  DCHECK(HoldingSpaceItemView::IsInstance(view));
  return static_cast<HoldingSpaceItemView*>(view);
}

// static
bool HoldingSpaceItemView::IsInstance(views::View* view) {
  return view->GetProperty(kIsHoldingSpaceItemViewProperty);
}

void HoldingSpaceItemView::AddLayerBeneathView(ui::Layer* layer) {
  ink_drop_container_->AddInkDropLayer(layer);
}

void HoldingSpaceItemView::RemoveLayerBeneathView(ui::Layer* layer) {
  ink_drop_container_->RemoveInkDropLayer(layer);
}

SkColor HoldingSpaceItemView::GetInkDropBaseColor() const {
  return AshColorProvider::Get()->GetRippleAttributes().base_color;
}

bool HoldingSpaceItemView::HandleAccessibleAction(
    const ui::AXActionData& action_data) {
  return delegate_->OnHoldingSpaceItemViewAccessibleAction(this, action_data) ||
         views::InkDropHostView::HandleAccessibleAction(action_data);
}

void HoldingSpaceItemView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  gfx::Rect bounds = GetLocalBounds();
  selected_layer_owner_->layer()->SetBounds(bounds);
  InvalidateLayer(selected_layer_owner_->layer());

  // The focus ring is painted just outside the bounds for this view.
  const float kFocusInsets = kHoldingSpaceFocusInsets -
                             (views::PlatformStyle::kFocusHaloThickness / 2.f);

  bounds.Inset(gfx::Insets(kFocusInsets));
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
  delegate_->OnHoldingSpaceItemViewGestureEvent(this, *event);
}

bool HoldingSpaceItemView::OnKeyPressed(const ui::KeyEvent& event) {
  return delegate_->OnHoldingSpaceItemViewKeyPressed(this, event);
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
  views::InkDropHostView::OnMouseEvent(event);
}

bool HoldingSpaceItemView::OnMousePressed(const ui::MouseEvent& event) {
  return delegate_->OnHoldingSpaceItemViewMousePressed(this, event);
}

void HoldingSpaceItemView::OnMouseReleased(const ui::MouseEvent& event) {
  delegate_->OnHoldingSpaceItemViewMouseReleased(this, event);
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
}

views::ToggleImageButton* HoldingSpaceItemView::AddPin(views::View* parent) {
  DCHECK(!pin_);

  pin_ = parent->AddChildView(std::make_unique<views::ToggleImageButton>());
  pin_->SetID(kHoldingSpaceItemPinButtonId);
  pin_->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  pin_->SetVisible(false);

  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);

  const gfx::ImageSkia unpinned_icon = gfx::CreateVectorIcon(
      views::kUnpinIcon, kHoldingSpaceIconSize, icon_color);
  const gfx::ImageSkia pinned_icon =
      gfx::CreateVectorIcon(views::kPinIcon, kHoldingSpaceIconSize, icon_color);

  pin_->SetImage(views::Button::STATE_NORMAL, unpinned_icon);
  pin_->SetToggledImage(views::Button::STATE_NORMAL, &pinned_icon);

  pin_->SetImageHorizontalAlignment(
      views::ToggleImageButton::HorizontalAlignment::ALIGN_CENTER);
  pin_->SetImageVerticalAlignment(
      views::ToggleImageButton::VerticalAlignment::ALIGN_MIDDLE);

  pin_->SetCallback(base::BindRepeating(&HoldingSpaceItemView::OnPinPressed,
                                        base::Unretained(this)));

  return pin_;
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
  canvas->DrawRoundRect(bounds, kHoldingSpaceCornerRadius, flags);
}

void HoldingSpaceItemView::OnPaintSelect(gfx::Canvas* canvas, gfx::Size size) {
  if (!selected_)
    return;

  const SkColor color = AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor);

  const SkColor overlay_color =
      SkColorSetA(color, kHoldingSpaceSelectedOverlayOpacity * 0xFF);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(overlay_color);

  gfx::Rect bounds = gfx::Rect(size);
  canvas->DrawRoundRect(bounds, kHoldingSpaceCornerRadius, flags);

  flags.setColor(color);
  flags.setStrokeWidth(views::PlatformStyle::kFocusHaloThickness);
  flags.setStyle(cc::PaintFlags::kStroke_Style);

  bounds.Inset(gfx::Insets(flags.getStrokeWidth() / 2));
  canvas->DrawRoundRect(bounds, kHoldingSpaceCornerRadius, flags);
}

void HoldingSpaceItemView::OnPinPressed() {
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
  if (!IsMouseHovered()) {
    pin_->SetVisible(false);
    OnPinVisiblityChanged(false);
    return;
  }

  const bool is_item_pinned =
      HoldingSpaceController::Get()->model()->ContainsItem(
          HoldingSpaceItem::Type::kPinnedFile, item()->file_path());

  pin_->SetToggled(!is_item_pinned);
  pin_->SetVisible(true);
  OnPinVisiblityChanged(true);
}

BEGIN_METADATA(HoldingSpaceItemView, views::InkDropHostView)
END_METADATA

}  // namespace ash
