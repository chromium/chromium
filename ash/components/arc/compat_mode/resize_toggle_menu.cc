// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/compat_mode/resize_toggle_menu.h"

#include "ash/components/arc/compat_mode/overlay_dialog.h"
#include "ash/components/arc/compat_mode/style/arc_color_provider.h"
#include "ash/public/cpp/arc_compat_mode_util.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/widget/widget.h"

namespace arc {

namespace {

class RoundedCornerBubbleDialogDelegateView
    : public views::BubbleDialogDelegateView {
  METADATA_HEADER(RoundedCornerBubbleDialogDelegateView,
                  views::BubbleDialogDelegateView)

 public:
  explicit RoundedCornerBubbleDialogDelegateView(int corner_radius)
      : corner_radius_(corner_radius) {}

  // views::View:
  void AddedToWidget() override {
    auto* const frame = GetBubbleFrameView();
    if (frame)
      frame->SetCornerRadius(corner_radius_);
  }

  base::WeakPtr<RoundedCornerBubbleDialogDelegateView> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  const int corner_radius_;
  base::WeakPtrFactory<RoundedCornerBubbleDialogDelegateView> weak_factory_{
      this};
};

BEGIN_METADATA(RoundedCornerBubbleDialogDelegateView)
END_METADATA

}  // namespace

ResizeToggleMenu::MenuButtonView::MenuButtonView(PressedCallback callback,
                                                 const gfx::VectorIcon& icon,
                                                 int title_string_id)
    : views::Button(std::move(callback)),
      icon_(icon),
      title_string_id_(title_string_id) {
  // Don't use FlexLayout here because it breaks the focus ring's bounds.
  // TODO(b/193195191): Investigate why we can't use FlexLayout.
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::TLBR(16, 0, 12, 0)));

  AddChildView(
      views::Builder<views::ImageView>()
          .CopyAddressTo(&icon_view_)
          .SetImageSize(gfx::Size(20, 20))
          .SetHorizontalAlignment(views::ImageView::Alignment::kCenter)
          .SetVerticalAlignment(views::ImageView::Alignment::kCenter)
          .SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 8, 0))
          .Build());
  const raw_ptr<views::Label> label =
      AddChildView(views::Builder<views::Label>()
                       .CopyAddressTo(&title_)
                       .SetBackgroundColor(SK_ColorTRANSPARENT)
                       .SetText(l10n_util::GetStringUTF16(title_string_id))
                       .SetVerticalAlignment(gfx::ALIGN_BOTTOM)
                       .SetLineHeight(16)
                       .SetMultiLine(true)
                       .SetAllowCharacterBreak(true)
                       .Build());
  ash::TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosButton2,
                                             *label);

  constexpr int kBorderThicknessDp = 1;
  const auto button_radius = 12;
  SetBorder(views::CreateRoundedRectBorder(kBorderThicknessDp, button_radius,
                                           gfx::kPlaceholderColor));
  SetBackground(views::CreateRoundedRectBackground(gfx::kPlaceholderColor,
                                                   button_radius));

  const int focus_ring_radius = 16;
  // With Jellyroll, the ring should have a 4dp gap from the view. Setting a
  // negative inset makes insets "outsets".
  const int focus_ring_inset = -4;
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetInstallFocusRingOnFocus(true);
  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(focus_ring_inset), focus_ring_radius);

  views::FocusRing::Get(this)->SetColorId(cros_tokens::kCrosSysFocusRing);

  GetViewAccessibility().SetRole(ax::mojom::Role::kMenuItem);
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(title_string_id_));
}

ResizeToggleMenu::MenuButtonView::~MenuButtonView() = default;

void ResizeToggleMenu::MenuButtonView::SetSelected(bool is_selected) {
  is_selected_ = is_selected;
  SetState(is_selected_ ? views::Button::ButtonState::STATE_DISABLED
                        : views::Button::ButtonState::STATE_NORMAL);
  UpdateColors();
}

void ResizeToggleMenu::MenuButtonView::OnThemeChanged() {
  views::Button::OnThemeChanged();
  UpdateColors();
}

gfx::Size ResizeToggleMenu::MenuButtonView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  constexpr int kWidth = 96;
  return gfx::Size(
      kWidth, GetLayoutManager()->GetPreferredHeightForWidth(this, kWidth));
}

void ResizeToggleMenu::MenuButtonView::UpdateColors() {
  if (!GetWidget())
    return;

  const auto* color_provider = GetColorProvider();

  const auto icon_color =
      is_selected_ ? color_provider->GetColor(cros_tokens::kCrosSysOnPrimary)
                   : color_provider->GetColor(cros_tokens::kCrosSysOnSurface);
  icon_view_->SetImage(gfx::CreateVectorIcon(*icon_, icon_color));

  const auto text_color =
      is_selected_ ? color_provider->GetColor(cros_tokens::kCrosSysOnPrimary)
                   : color_provider->GetColor(cros_tokens::kCrosSysOnSurface);
  title_->SetEnabledColor(text_color);

  const auto background_color =
      is_selected_
          ? color_provider->GetColor(cros_tokens::kCrosSysPrimary)
          : color_provider->GetColor(cros_tokens::kCrosSysSystemOnBase);
  background()->SetNativeControlColor(background_color);

  const auto border_color = SK_ColorTRANSPARENT;
  GetBorder()->set_color(border_color);
}

BEGIN_METADATA(ResizeToggleMenu, MenuButtonView)
END_METADATA

ResizeToggleMenu::ResizeToggleMenu(
    base::OnceClosure on_bubble_widget_closing_callback,
    views::Widget* widget,
    ArcResizeLockPrefDelegate* pref_delegate)
    : on_bubble_widget_closing_callback_(
          std::move(on_bubble_widget_closing_callback)),
      widget_(widget),
      pref_delegate_(pref_delegate) {
  aura::Window* const window = widget->GetNativeWindow();
  // Don't show the menu in maximized or fullscreen.
  const ui::mojom::WindowShowState state =
      window->GetProperty(aura::client::kShowStateKey);
  if (state == ui::mojom::WindowShowState::kFullscreen ||
      state == ui::mojom::WindowShowState::kMaximized) {
    return;
  }

  window_observation_.Observe(window);

  bubble_widget_ =
      views::BubbleDialogDelegateView::CreateBubble(MakeBubbleDelegateView(
          widget_, GetAnchorRect(),
          base::BindRepeating(&ResizeToggleMenu::ApplyResizeCompatMode,
                              base::Unretained(this))));
  widget_observations_.AddObservation(widget_.get());
  widget_observations_.AddObservation(bubble_widget_.get());
  OverlayDialog::Show(widget_->GetNativeWindow(),
                      base::BindOnce(&ResizeToggleMenu::CloseBubble,
                                     weak_ptr_factory_.GetWeakPtr()),
                      /*dialog_view=*/nullptr);
  bubble_widget_->Show();
}

ResizeToggleMenu::~ResizeToggleMenu() {
  CloseBubble();
}

void ResizeToggleMenu::OnWidgetClosing(views::Widget* widget) {
  if (widget == bubble_widget_ && on_bubble_widget_closing_callback_) {
    std::move(on_bubble_widget_closing_callback_).Run();
  }

  OverlayDialog::CloseIfAny(widget_->GetNativeWindow());
  widget_observations_.RemoveAllObservations();
  widget_ = nullptr;
  bubble_widget_ = nullptr;
}

void ResizeToggleMenu::OnWidgetBoundsChanged(views::Widget* widget,
                                             const gfx::Rect& new_bounds) {
  if (widget != widget_)
    return;

  DCHECK(bubble_widget_);
  bubble_widget_->widget_delegate()->AsBubbleDialogDelegate()->SetAnchorRect(
      GetAnchorRect());

  UpdateSelectedButton();
}

void ResizeToggleMenu::OnWindowPropertyChanged(aura::Window* window,
                                               const void* key,
                                               intptr_t old) {
  DCHECK(window_observation_.IsObservingSource(window));
  if (key == aura::client::kShowStateKey) {
    const ui::mojom::WindowShowState state =
        window->GetProperty(aura::client::kShowStateKey);
    if (state == ui::mojom::WindowShowState::kFullscreen ||
        state == ui::mojom::WindowShowState::kMaximized) {
      CloseBubble();
    }
  } else if (key == ash::kArcResizeLockTypeKey) {
    UpdateSelectedButton();
  }
}

void ResizeToggleMenu::OnWindowDestroying(aura::Window* window) {
  DCHECK(window_observation_.IsObservingSource(window));
  window_observation_.Reset();
}

gfx::Rect ResizeToggleMenu::GetAnchorRect() const {
  DCHECK(widget_);
  const gfx::Rect client_view_rect =
      widget_->client_view()->GetBoundsInScreen();
  // Anchored to the top edge of the client_view with padding.
  constexpr auto kMarginTopDp = 8;
  return gfx::Rect(client_view_rect.x(), client_view_rect.y() + kMarginTopDp,
                   client_view_rect.width(), 0);
}

std::unique_ptr<views::BubbleDialogDelegateView>
ResizeToggleMenu::MakeBubbleDelegateView(
    views::Widget* parent,
    gfx::Rect anchor_rect,
    base::RepeatingCallback<void(ash::ResizeCompatMode)> command_handler) {
  const int kCornerRadius = 12;

  auto delegate_view =
      std::make_unique<RoundedCornerBubbleDialogDelegateView>(kCornerRadius);
  bubble_view_ = delegate_view->GetWeakPtr();

  // Setup delegate.
  delegate_view->SetArrow(views::BubbleBorder::Arrow::TOP_CENTER);
  delegate_view->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  delegate_view->set_parent_window(parent->GetNativeView());
  delegate_view->set_title_margins(gfx::Insets());
  delegate_view->set_margins(gfx::Insets());
  delegate_view->SetAnchorRect(anchor_rect);
  delegate_view->SetTitle(
      l10n_util::GetStringUTF16(IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_TITLE));
  delegate_view->SetShowTitle(false);
  delegate_view->SetAccessibleWindowRole(ax::mojom::Role::kMenu);
  // Clear root view's background color. We use the color in
  // `background_view`.
  delegate_view->set_color(SK_ColorTRANSPARENT);

  // Setup view.
  delegate_view->SetUseDefaultFillLayout(true);

  delegate_view->SetBorder(std::make_unique<views::HighlightBorder>(
      kCornerRadius, views::HighlightBorder::Type::kHighlightBorderNoShadow));

  // Add empty view for background blur.
  views::View* background_view = nullptr;
  delegate_view->AddChildView(
      views::Builder<views::View>()
          .CopyAddressTo(&background_view)
          .SetUseDefaultFillLayout(true)
          .SetBackground(views::CreateThemedRoundedRectBackground(
              cros_tokens::kCrosSysSystemBaseElevated, kCornerRadius))
          .Build());

  background_view->SetPaintToLayer();
  background_view->layer()->SetBackgroundBlur(
      ash::ColorProvider::kBackgroundBlurSigma);
  background_view->layer()->SetBackdropFilterQuality(
      ash::ColorProvider::kBackgroundBlurQuality);
  background_view->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kCornerRadius));
  background_view->layer()->SetFillsBoundsOpaquely(false);

  auto* const container_view =
      delegate_view->AddChildView(std::make_unique<views::View>());

  auto* const provider = views::LayoutProvider::Get();
  container_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(16),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));

  const auto add_menu_button = [&container_view, &command_handler](
                                   ash::ResizeCompatMode command_id,
                                   const gfx::VectorIcon& icon, int string_id) {
    return container_view->AddChildView(std::make_unique<MenuButtonView>(
        base::BindRepeating(command_handler, command_id), icon, string_id));
  };
  phone_button_ =
      add_menu_button(ash::ResizeCompatMode::kPhone, ash::kSystemMenuPhoneIcon,
                      IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_PORTRAIT);
  tablet_button_ = add_menu_button(
      ash::ResizeCompatMode::kTablet, ash::kSystemMenuTabletIcon,
      IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_LANDSCAPE);
  resizable_button_ = add_menu_button(
      ash::ResizeCompatMode::kResizable, ash::kAppCompatResizableIcon,
      IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_RESIZABLE);

  UpdateSelectedButton();

  // We need to ensure that the layer is non-opaque for popup animation.
  delegate_view->SetPaintToLayer();
  delegate_view->layer()->SetFillsBoundsOpaquely(false);

  // Note this view needs to be set to paint to layer so other view won't
  // paint over it.
  container_view->SetPaintToLayer();
  container_view->layer()->SetFillsBoundsOpaquely(false);

  return delegate_view;
}

void ResizeToggleMenu::UpdateSelectedButton() {
  // No need to update the button states if the widget is (being) closed.
  if (!widget_)
    return;

  const auto selected_mode = ash::compat_mode_util::PredictCurrentMode(widget_);
  phone_button_->SetSelected(selected_mode == ash::ResizeCompatMode::kPhone);
  tablet_button_->SetSelected(selected_mode == ash::ResizeCompatMode::kTablet);
  resizable_button_->SetSelected(selected_mode ==
                                 ash::ResizeCompatMode::kResizable);
}

void ResizeToggleMenu::ApplyResizeCompatMode(ash::ResizeCompatMode mode) {
  switch (mode) {
    case ash::ResizeCompatMode::kPhone:
      ResizeLockToPhone(widget_, pref_delegate_);
      break;
    case ash::ResizeCompatMode::kTablet:
      ResizeLockToTablet(widget_, pref_delegate_);
      break;
    case ash::ResizeCompatMode::kResizable:
      EnableResizingWithConfirmationIfNeeded(widget_, pref_delegate_);
      break;
  }

  auto_close_closure_.Reset(base::BindOnce(&ResizeToggleMenu::CloseBubble,
                                           weak_ptr_factory_.GetWeakPtr()));
  constexpr auto kAutoCloseDelay = base::Seconds(2);
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, auto_close_closure_.callback(), kAutoCloseDelay);
}

bool ResizeToggleMenu::IsBubbleShown() const {
  return bubble_view_ && bubble_view_->GetWidget();
}

void ResizeToggleMenu::CloseBubble() {
  if (!bubble_widget_ || bubble_widget_->IsClosed())
    return;

  bubble_widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

}  // namespace arc
