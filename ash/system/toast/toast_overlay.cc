// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/toast_overlay.h"

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/work_area_insets.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

// Duration of slide animation when overlay is shown or hidden.
constexpr int kSlideAnimationDurationMs = 100;

// These values are in DIP.
constexpr int kToastCornerRounding = 16;
constexpr int kToastHeight = 32;
constexpr int kToastHorizontalSpacing = 16;
constexpr int kToastMaximumWidth = 512;
constexpr int kToastMinimumWidth = 288;
constexpr int kToastButtonMaximumWidth = 160;

// Returns the work area bounds for the root window where new windows are added
// (including new toasts).
gfx::Rect GetUserWorkAreaBounds() {
  return WorkAreaInsets::ForWindow(Shell::GetRootWindowForNewWindows())
      ->user_work_area_bounds();
}

///////////////////////////////////////////////////////////////////////////////
//  ToastOverlayLabel
class ToastOverlayLabel : public views::Label {
 public:
  explicit ToastOverlayLabel(const base::string16& label)
      : Label(label, CONTEXT_TOAST_OVERLAY) {
    SetHorizontalAlignment(gfx::ALIGN_LEFT);
    SetAutoColorReadabilityEnabled(false);
    SetMultiLine(true);
    SetMaxLines(2);
    SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
    SetSubpixelRenderingEnabled(false);

    int vertical_spacing =
        std::max((kToastHeight - GetPreferredSize().height()) / 2, 0);
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets(vertical_spacing, kToastHorizontalSpacing)));
  }

  ~ToastOverlayLabel() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ToastOverlayLabel);
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//  ToastDisplayObserver
class ToastOverlay::ToastDisplayObserver : public display::DisplayObserver {
 public:
  ToastDisplayObserver(ToastOverlay* overlay) : overlay_(overlay) {
    display::Screen::GetScreen()->AddObserver(this);
  }

  ~ToastDisplayObserver() override {
    display::Screen::GetScreen()->RemoveObserver(this);
  }

  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override {
    overlay_->UpdateOverlayBounds();
  }

 private:
  ToastOverlay* const overlay_;
  DISALLOW_COPY_AND_ASSIGN(ToastDisplayObserver);
};

///////////////////////////////////////////////////////////////////////////////
//  ToastOverlayButton
class ToastOverlayButton : public views::LabelButton {
 public:
  ToastOverlayButton(views::ButtonListener* listener,
                     const base::string16& text)
      : views::LabelButton(listener, text, CONTEXT_TOAST_OVERLAY) {
    SetInkDropMode(InkDropMode::ON);
    SetHasInkDropActionOnClick(true);
    const auto* color_provider = AshColorProvider::Get();
    SetInkDropBaseColor(color_provider->GetRippleAttributes().base_color);
    SetEnabledTextColors(color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kButtonLabelColorBlue));

    // Treat the space below the baseline as a margin.
    int vertical_spacing =
        std::max((kToastHeight - GetPreferredSize().height()) / 2, 0);
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets(vertical_spacing, kToastHorizontalSpacing)));

    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                  kToastCornerRounding);
  }

  ~ToastOverlayButton() override = default;

 protected:
  // views::LabelButton:
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    return std::make_unique<views::InkDropHighlight>(
        gfx::SizeF(GetLocalBounds().size()), GetInkDropBaseColor());
  }

 private:
  friend class ToastOverlay;  // for ToastOverlay::ClickDismissButtonForTesting.

  DISALLOW_COPY_AND_ASSIGN(ToastOverlayButton);
};

///////////////////////////////////////////////////////////////////////////////
//  ToastOverlayView
class ToastOverlayView : public views::View, public views::ButtonListener {
 public:
  // This object is not owned by the views hierarchy or by the widget.
  ToastOverlayView(ToastOverlay* overlay,
                   const base::string16& text,
                   const base::Optional<base::string16>& dismiss_text,
                   const bool is_managed)
      : overlay_(overlay) {
    SetPaintToLayer();
    SetBackground(
        views::CreateSolidBackground(AshColorProvider::Get()->GetBaseLayerColor(
            AshColorProvider::BaseLayerType::kTransparent80)));
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(kToastCornerRounding));
    layer()->SetBackgroundBlur(
        static_cast<float>(AshColorProvider::LayerBlurSigma::kBlurDefault));

    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));

    int icon_width = 0;
    if (is_managed) {
      auto* icon = new views::ImageView;
      icon->SetImage(gfx::CreateVectorIcon(
          kUnifiedMenuManagedIcon,
          AshColorProvider::Get()->GetContentLayerColor(
              AshColorProvider::ContentLayerType::kIconColorPrimary)));
      icon->SetBorder(views::CreateEmptyBorder(
          gfx::Insets(kToastHorizontalSpacing, kToastHorizontalSpacing,
                      kToastHorizontalSpacing, /*right=*/0)));
      AddChildView(icon);
      icon_width = icon->GetPreferredSize().width() + kToastHorizontalSpacing;
    }

    auto* label = new ToastOverlayLabel(text);
    AddChildView(label);
    label->SetMaximumWidth(GetMaximumSize().width() - icon_width);
    layout->SetFlexForView(label, 1);

    if (!dismiss_text.has_value())
      return;

    button_ = new ToastOverlayButton(
        this, dismiss_text.value().empty()
                  ? l10n_util::GetStringUTF16(IDS_ASH_TOAST_DISMISS_BUTTON)
                  : dismiss_text.value());

    const int button_width =
        std::min(button_->GetPreferredSize().width(), kToastButtonMaximumWidth);
    button_->SetMaxSize(gfx::Size(button_width, GetMaximumSize().height()));
    label->SetMaximumWidth(GetMaximumSize().width() - button_width -
                           icon_width - kToastHorizontalSpacing * 2 -
                           kToastHorizontalSpacing * 2);
    AddChildView(button_);
  }

  ~ToastOverlayView() override = default;

  ToastOverlayButton* button() { return button_; }

 private:
  // views::View:
  gfx::Size GetMinimumSize() const override {
    return gfx::Size(kToastMinimumWidth, kToastHeight);
  }

  gfx::Size GetMaximumSize() const override {
    return gfx::Size(kToastMaximumWidth, GetUserWorkAreaBounds().height() -
                                             ToastOverlay::kOffset * 2);
  }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    DCHECK_EQ(button_, sender);
    overlay_->Show(false);
  }

  ToastOverlay* overlay_ = nullptr;       // weak
  ToastOverlayButton* button_ = nullptr;  // weak

  DISALLOW_COPY_AND_ASSIGN(ToastOverlayView);
};

///////////////////////////////////////////////////////////////////////////////
//  ToastOverlay
ToastOverlay::ToastOverlay(Delegate* delegate,
                           const base::string16& text,
                           base::Optional<base::string16> dismiss_text,
                           bool show_on_lock_screen,
                           bool is_managed)
    : delegate_(delegate),
      text_(text),
      dismiss_text_(dismiss_text),
      overlay_widget_(new views::Widget),
      overlay_view_(new ToastOverlayView(this, text, dismiss_text, is_managed)),
      display_observer_(std::make_unique<ToastDisplayObserver>(this)),
      widget_size_(overlay_view_->GetPreferredSize()) {
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.name = "ToastOverlay";
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.accept_events = true;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.bounds = CalculateOverlayBounds();
  // Show toasts above the app list and below the lock screen.
  params.parent = Shell::GetRootWindowForNewWindows()->GetChildById(
      show_on_lock_screen ? kShellWindowId_LockSystemModalContainer
                          : kShellWindowId_SystemModalContainer);
  overlay_widget_->Init(std::move(params));
  overlay_widget_->SetVisibilityChangedAnimationsEnabled(true);
  overlay_widget_->SetContentsView(overlay_view_.get());
  UpdateOverlayBounds();

  aura::Window* overlay_window = overlay_widget_->GetNativeWindow();
  ::wm::SetWindowVisibilityAnimationType(
      overlay_window, ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL);
  ::wm::SetWindowVisibilityAnimationDuration(
      overlay_window,
      base::TimeDelta::FromMilliseconds(kSlideAnimationDurationMs));

  keyboard::KeyboardUIController::Get()->AddObserver(this);
}

ToastOverlay::~ToastOverlay() {
  keyboard::KeyboardUIController::Get()->RemoveObserver(this);
  overlay_widget_->Close();
}

void ToastOverlay::Show(bool visible) {
  if (overlay_widget_->GetLayer()->GetTargetVisibility() == visible)
    return;

  ui::LayerAnimator* animator = overlay_widget_->GetLayer()->GetAnimator();
  DCHECK(animator);

  base::TimeDelta original_duration = animator->GetTransitionDuration();
  ui::ScopedLayerAnimationSettings animation_settings(animator);
  // ScopedLayerAnimationSettings ctor changes the transition duration, so
  // change it back to the original value (should be zero).
  animation_settings.SetTransitionDuration(original_duration);

  animation_settings.AddObserver(this);

  if (visible) {
    overlay_widget_->Show();

    // Notify accessibility about the overlay.
    overlay_view_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, false);
  } else {
    overlay_widget_->Hide();
  }
}

void ToastOverlay::UpdateOverlayBounds() {
  overlay_widget_->SetBounds(CalculateOverlayBounds());
}

gfx::Rect ToastOverlay::CalculateOverlayBounds() {
  gfx::Rect bounds = GetUserWorkAreaBounds();
  int target_y =
      bounds.bottom() - widget_size_.height() - ToastOverlay::kOffset;
  bounds.ClampToCenteredSize(widget_size_);
  bounds.set_y(target_y);
  return bounds;
}

void ToastOverlay::OnImplicitAnimationsScheduled() {}

void ToastOverlay::OnImplicitAnimationsCompleted() {
  if (!overlay_widget_->GetLayer()->GetTargetVisibility())
    delegate_->OnClosed();
}

void ToastOverlay::OnKeyboardOccludedBoundsChanged(
    const gfx::Rect& new_bounds_in_screen) {
  // TODO(https://crbug.com/943446): Observe changes in user work area bounds
  // directly instead of listening for keyboard bounds changes.
  UpdateOverlayBounds();
}

views::Widget* ToastOverlay::widget_for_testing() {
  return overlay_widget_.get();
}

ToastOverlayButton* ToastOverlay::dismiss_button_for_testing() {
  return overlay_view_->button();
}

void ToastOverlay::ClickDismissButtonForTesting(const ui::Event& event) {
  DCHECK(overlay_view_->button());
  overlay_view_->button()->NotifyClick(event);
}

}  // namespace ash
