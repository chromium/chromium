// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/tab_app_selection_host.h"

#include "ash/accessibility/scoped_a11y_override_window_setter.h"
#include "ash/birch/birch_coral_item.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/style/icon_button.h"
#include "ash/wm/overview/birch/birch_chip_button.h"
#include "ash/wm/overview/birch/birch_chip_button_base.h"
#include "ash/wm/overview/birch/tab_app_selection_view.h"
#include "ash/wm/window_properties.h"
#include "base/metrics/histogram_functions.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/events/event_handler.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"

namespace ash {

constexpr base::TimeDelta kFadeAnimationDuration = base::Milliseconds(200);
constexpr base::TimeDelta kSlideAnimationDuration = base::Milliseconds(300);

// Pre target event handler that handles closing the widget if a mouse or touch
// press event is seen outside the coral chip bounds.
class TabAppSelectionHost::SelectionHostHider : public ui::EventHandler {
 public:
  explicit SelectionHostHider(TabAppSelectionHost* owner) : owner_(owner) {
    Shell::Get()->AddPreTargetHandler(this);
  }
  SelectionHostHider(const SelectionHostHider&) = delete;
  SelectionHostHider& operator=(const SelectionHostHider&) = delete;
  ~SelectionHostHider() override { Shell::Get()->RemovePreTargetHandler(this); }

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    if (event->type() == ui::EventType::kMousePressed ||
        event->type() == ui::EventType::kTouchPressed) {
      // Ignore all events if the host widget is not visible.
      if (!owner_->IsVisible()) {
        return;
      }

      gfx::Point event_screen_point = event->AsLocatedEvent()->root_location();
      wm::ConvertPointToScreen(
          static_cast<aura::Window*>(event->target())->GetRootWindow(),
          &event_screen_point);
      // Unless the event is on the host widget, slide it out and stop the event
      // from propagating.
      if (!owner_->GetWindowBoundsInScreen().Contains(event_screen_point)) {
        owner_->SlideOut();
        event->SetHandled();
        event->StopPropagation();
      }
    }
  }
  std::string_view GetLogContext() const override {
    return "TabAppSelectionHost::SelectionHostHider";
  }

 private:
  const raw_ptr<TabAppSelectionHost> owner_;
};

TabAppSelectionHost::TabAppSelectionHost(BirchChipButton* coral_chip)
    : hider_(std::make_unique<SelectionHostHider>(this)),
      owner_(coral_chip),
      scoped_a11y_overrider_(
          std::make_unique<ScopedA11yOverrideWindowSetter>()) {
  aura::Window* parent = coral_chip->GetWidget()->GetNativeWindow()->parent();
  using InitParams = views::Widget::InitParams;
  InitParams params(InitParams::CLIENT_OWNS_WIDGET, InitParams::TYPE_MENU);
  params.accept_events = true;
  params.activatable = InitParams::Activatable::kNo;
  params.autosize = true;
  params.name = "TabAppSelectionMenu";
  params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);
  params.init_properties_container.SetProperty(kOverviewUiKey, true);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent = parent;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;

  Init(std::move(params));
  SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
  SetContentsView(std::make_unique<TabAppSelectionView>(
      static_cast<BirchCoralItem*>(coral_chip->GetItem())->group_id(),
      base::BindRepeating(&TabAppSelectionHost::OnItemRemoved,
                          base::Unretained(this))));
  widget_delegate()->set_desired_bounds_delegate(base::BindRepeating(
      &TabAppSelectionHost::GetDesiredBoundsInScreen, base::Unretained(this)));
  SetBounds(GetDesiredBoundsInScreen());

  // Stack the widget below the coral chip so it slides under the chip.
  parent->StackChildBelow(GetNativeWindow(),
                          coral_chip->GetWidget()->GetNativeWindow());
}

TabAppSelectionHost::~TabAppSelectionHost() {
  base::UmaHistogramExactLinear("Ash.Birch.Coral.ClusterItemRemoved",
                                number_of_removed_items_, /*exclusive_max=*/9);
}

void TabAppSelectionHost::ProcessKeyEvent(ui::KeyEvent* event) {
  if (event->type() != ui::EventType::kKeyPressed) {
    return;
  }

  event->SetHandled();
  event->StopPropagation();

  if (event->key_code() == ui::VKEY_ESCAPE) {
    Hide();
    return;
  }

  views::AsViewClass<TabAppSelectionView>(GetContentsView())
      ->ProcessKeyEvent(event);
}

void TabAppSelectionHost::OnItemRemoved() {
  number_of_removed_items_++;
  owner_->ReloadIcon();
}

void TabAppSelectionHost::SlideOut() {
  const gfx::Rect chip_bounds = owner_->GetBoundsInScreen();
  const gfx::Rect selection_bounds = GetWindowBoundsInScreen();
  ui::Layer* layer = GetLayer();

  auto on_animation_end = base::BindRepeating(
      [](base::WeakPtr<views::Widget> self) {
        if (self) {
          self->GetLayer()->SetOpacity(1.f);
          self->GetLayer()->SetTransform(gfx::Transform());
          self->GetLayer()->SetClipRect(gfx::Rect());
          self->Hide();
        }
      },
      GetWeakPtr());

  // Slide the widget into the coral chip. We apply a clip as well since the
  // contents view is almost always taller than the coral chip.
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(on_animation_end)
      .OnAborted(on_animation_end)
      .Once()
      .SetOpacity(layer, 1.f)
      .At(base::TimeDelta())
      .SetDuration(kFadeAnimationDuration)
      .SetOpacity(layer, 0.f)
      .At(base::TimeDelta())
      .SetDuration(kSlideAnimationDuration)
      .SetTransform(layer,
                    gfx::Transform::MakeTranslation(
                        0, chip_bounds.y() - selection_bounds.y()),
                    gfx::Tween::EASE_IN_OUT_EMPHASIZED)
      .SetClipRect(layer,
                   gfx::Rect(selection_bounds.width(), chip_bounds.height()),
                   gfx::Tween::EASE_IN_OUT_EMPHASIZED);
}

void TabAppSelectionHost::RemoveItem(std::string_view identifier) {
  views::AsViewClass<TabAppSelectionView>(GetContentsView())
      ->RemoveItemBySystem(identifier);
}

void TabAppSelectionHost::OnNativeWidgetVisibilityChanged(bool visible) {
  views::Widget::OnNativeWidgetVisibilityChanged(visible);
  views::AsViewClass<IconButton>(owner_->addon_view())
      ->SetVectorIcon(visible ? vector_icons::kCaretDownIcon
                              : vector_icons::kCaretUpIcon);
  owner_->OnSelectionWidgetVisibilityChanged();
  scoped_a11y_overrider_->MaybeUpdateA11yOverrideWindow(
      visible ? GetNativeWindow() : nullptr);

  if (visible) {
    base::UmaHistogramBoolean("Ash.Birch.Coral.ClusterExpanded", true);
    GetContentsView()->GetViewAccessibility().NotifyEvent(
        ax::mojom::Event::kMenuStart);

    auto on_animation_end = base::BindRepeating(
        [](base::WeakPtr<views::Widget> self) {
          if (self) {
            self->GetLayer()->SetOpacity(1.f);
            self->GetLayer()->SetTransform(gfx::Transform());
            self->GetLayer()->SetClipRect(gfx::Rect());
          }
        },
        GetWeakPtr());

    // Update the bounds before showing up.
    SetBounds(GetDesiredBoundsInScreen());

    // Slide the widget out of the coral chip. We apply a clip as well since the
    // contents view is almost always taller than the coral chip.
    const gfx::Rect chip_bounds = owner_->GetBoundsInScreen();
    const gfx::Rect selection_bounds = GetWindowBoundsInScreen();
    ui::Layer* layer = GetLayer();
    views::AnimationBuilder()
        .SetPreemptionStrategy(
            ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
        .OnEnded(on_animation_end)
        .OnAborted(on_animation_end)
        .Once()
        .SetOpacity(layer, 0.f)
        .SetTransform(layer, gfx::Transform::MakeTranslation(
                                 0, chip_bounds.y() - selection_bounds.y()))
        .SetClipRect(layer,
                     gfx::Rect(selection_bounds.width(), chip_bounds.height()))
        .At(base::TimeDelta())
        .SetDuration(kFadeAnimationDuration)
        .SetOpacity(layer, 1.f)
        .At(base::TimeDelta())
        .SetDuration(kSlideAnimationDuration)
        .SetTransform(layer, gfx::Transform(),
                      gfx::Tween::EASE_IN_OUT_EMPHASIZED)
        .SetClipRect(
            layer,
            gfx::Rect(selection_bounds.width(), selection_bounds.height()),
            gfx::Tween::EASE_IN_OUT_EMPHASIZED);
  } else {
    views::AsViewClass<TabAppSelectionView>(GetContentsView())
        ->ClearSelection();
  }
}

gfx::Rect TabAppSelectionHost::GetDesiredBoundsInScreen() {
  const int preferred_height = GetContentsView()->GetPreferredSize().height();
  gfx::Rect selector_bounds = owner_->GetBoundsInScreen();
  selector_bounds.set_y(selector_bounds.y() - preferred_height);
  selector_bounds.set_height(preferred_height);
  return selector_bounds;
}

}  // namespace ash
