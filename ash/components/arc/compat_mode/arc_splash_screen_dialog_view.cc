// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/compat_mode/arc_splash_screen_dialog_view.h"

#include <memory>

#include "ash/components/arc/compat_mode/overlay_dialog.h"
#include "ash/components/arc/compat_mode/style/arc_color_provider.h"
#include "ash/components/arc/vector_icons/vector_icons.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/style/pill_button.h"
#include "ash/style/typography.h"
#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/scoped_multi_source_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/frame/caption_buttons/frame_center_button.h"
#include "chromeos/ui/frame/default_frame_header.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace arc {

namespace {

// Draws the blue-ish highlight border to the parent view according to the
// highlight path.
class HighlightBorder : public views::View {
  METADATA_HEADER(HighlightBorder, views::View)

 public:
  HighlightBorder() = default;
  HighlightBorder(const HighlightBorder&) = delete;
  HighlightBorder& operator=(const HighlightBorder&) = delete;
  ~HighlightBorder() override = default;

  // views::View:
  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    InvalidateLayout();
    SchedulePaint();
  }

  void Layout(PassKey) override {
    auto bounds = parent()->GetLocalBounds();
    bounds.Inset(gfx::Insets(views::FocusRing::kDefaultHaloInset));
    SetBoundsRect(bounds);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);

    const auto rrect =
        views::HighlightPathGenerator::GetRoundRectForView(parent());
    if (!rrect)
      return;
    auto rect = (*rrect).rect();
    View::ConvertRectToTarget(parent(), this, &rect);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(
        GetColorProvider()->GetColor(cros_tokens::kCrosSysFocusRing));
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(views::FocusRing::kDefaultHaloThickness);
    canvas->DrawRoundRect(rect, (*rrect).GetSimpleRadius(), flags);
  }
};

BEGIN_METADATA(HighlightBorder)
END_METADATA

}  // namespace

class ArcSplashScreenDialogView::ArcSplashScreenWindowObserver
    : public aura::WindowObserver {
 public:
  ArcSplashScreenWindowObserver(aura::Window* window,
                                base::RepeatingClosure on_close_callback)
      : on_close_callback_(on_close_callback) {
    window_observation_.Observe(window);
  }

  ArcSplashScreenWindowObserver(const ArcSplashScreenWindowObserver&) = delete;
  ArcSplashScreenWindowObserver& operator=(
      const ArcSplashScreenWindowObserver&) = delete;
  ~ArcSplashScreenWindowObserver() override = default;

 private:
  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key != aura::client::kShowStateKey)
      return;

    ui::mojom::WindowShowState state =
        window->GetProperty(aura::client::kShowStateKey);
    if (state == ui::mojom::WindowShowState::kFullscreen ||
        state == ui::mojom::WindowShowState::kMaximized) {
      // Run the callback when window is fullscreen or maximized.
      on_close_callback_.Run();
    }
  }

  void OnWindowDestroying(aura::Window* window) override {
    window_observation_.Reset();
  }

  base::RepeatingClosure on_close_callback_;
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

ArcSplashScreenDialogView::ArcSplashScreenDialogView(
    base::OnceClosure close_callback,
    aura::Window* parent,
    views::View* anchor,
    bool is_for_unresizable)
    : anchor_(anchor),
      close_callback_(std::move(close_callback)),
      background_color_id_(cros_tokens::kCrosSysDialogContainer) {
  // Setup delegate.
  SetArrow(views::BubbleBorder::Arrow::BOTTOM_CENTER);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_parent_window(parent);
  set_title_margins(gfx::Insets());
  set_margins(gfx::Insets());
  SetAnchorView(anchor_);
  SetTitle(l10n_util::GetStringUTF16(IDS_ARC_COMPAT_MODE_SPLASH_SCREEN_TITLE));
  SetShowTitle(false);
  SetAccessibleWindowRole(ax::mojom::Role::kDialog);
  // For handling the case when Esc key is pressed.
  SetCancelCallback(
      base::BindOnce(&ArcSplashScreenDialogView::OnCloseButtonClicked,
                     weak_ptr_factory_.GetWeakPtr()));
  set_adjust_if_offscreen(false);
  set_close_on_deactivate(false);

  // Setup views.
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(gfx::Insets::TLBR(32, 32, 32, 28))
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred,
                                   /*adjust_height_for_width=*/true));

  auto image =
      ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
          IDR_ARC_COMPAT_MODE_SPLASH_SCREEN_IMAGE);
  AddChildView(
      views::Builder<views::ImageView>()  // Logo
          .SetImage(image)
          .SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 16, 0))
          .Build());

  const raw_ptr<views::Label> title_label =
      AddChildView(views::Builder<views::Label>()  // Header
                       .SetText(l10n_util::GetStringUTF16(
                           IDS_ARC_COMPAT_MODE_SPLASH_SCREEN_TITLE))
                       .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
                       .SetHorizontalAlignment(gfx::ALIGN_CENTER)
                       .SetAllowCharacterBreak(true)
                       .SetMultiLine(true)
                       .SetProperty(views::kMarginsKey, gfx::Insets::VH(16, 0))
                       .Build());
  ash::TypographyProvider::Get()->StyleLabel(
      ash::TypographyToken::kCrosDisplay7, *title_label);
  title_label->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);

  const raw_ptr<views::Label> body_label = AddChildView(
      views::Builder<views::Label>()  // Body
          .SetText(is_for_unresizable
                       ? l10n_util::GetStringUTF16(
                             IDS_ARC_COMPAT_MODE_SPLASH_SCREEN_BODY_UNRESIZABLE)
                       : l10n_util::GetStringFUTF16(
                             IDS_ARC_COMPAT_MODE_SPLASH_SCREEN_BODY,
                             parent->GetTitle()))
          .SetTextStyle(views::style::STYLE_SECONDARY)
          .SetTextContext(views::style::TextContext::CONTEXT_DIALOG_BODY_TEXT)
          .SetHorizontalAlignment(gfx::ALIGN_CENTER)
          .SetMultiLine(true)
          .Build());
  ash::TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosBody1,
                                             *body_label);
  body_label->SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant);

  AddChildView(
      views::Builder<ash::PillButton>()  // Close button
          .CopyAddressTo(&close_button_)
          .SetCallback(base::BindRepeating(
              &ArcSplashScreenDialogView::OnCloseButtonClicked,
              base::Unretained(this)))
          .SetText(l10n_util::GetStringUTF16(
              IDS_ARC_COMPAT_MODE_SPLASH_SCREEN_CLOSE))
          .SetPillButtonType(ash::PillButton::kPrimaryLargeWithoutIcon)
          .SetIsDefault(true)
          .SetProperty(views::kMarginsKey, gfx::Insets::TLBR(32, 0, 0, 0))
          .Build());

  // Setup highlight border.
  highlight_border_ =
      anchor_->AddChildView(std::make_unique<HighlightBorder>());

  // Observe anchor and its highlight to be notified when it's destroyed.
  anchor_highlight_observations_.AddObservation(anchor_.get());
  anchor_highlight_observations_.AddObservation(highlight_border_.get());

  // Add window observer.
  window_observer_ = std::make_unique<ArcSplashScreenWindowObserver>(
      parent,
      base::BindRepeating(&ArcSplashScreenDialogView::OnCloseButtonClicked,
                          base::Unretained(this)));

  activation_observation_.Observe(
      wm::GetActivationClient(parent_window()->GetRootWindow()));
}

ArcSplashScreenDialogView::~ArcSplashScreenDialogView() = default;

gfx::Size ArcSplashScreenDialogView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  auto width = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DistanceMetric::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  const auto* widget = GetWidget();
  if (widget && widget->parent()) {
    const int kHorizontalMarginDp = 36;
    width = std::min(widget->parent()->GetWindowBoundsInScreen().width() -
                         kHorizontalMarginDp * 2,
                     width);
  }
  return gfx::Size(width,
                   GetLayoutManager()->GetPreferredHeightForWidth(this, width));
}

gfx::Rect ArcSplashScreenDialogView::GetBubbleBounds() {
  gfx::Rect bubble_bounds = BubbleDialogDelegate::GetBubbleBounds();
  constexpr int kMarginTopDp = 8;
  bubble_bounds.Offset(0, kMarginTopDp);
  return bubble_bounds;
}

void ArcSplashScreenDialogView::AddedToWidget() {
  const int kCornerRadius = 20;
  auto* const frame = GetBubbleFrameView();
  if (frame)
    frame->SetCornerRadius(kCornerRadius);
}

void ArcSplashScreenDialogView::OnThemeChanged() {
  views::BubbleDialogDelegateView::OnThemeChanged();
  set_color(GetColorProvider()->GetColor(background_color_id_));
}

void ArcSplashScreenDialogView::OnViewIsDeleting(View* observed_view) {
  if (observed_view == anchor_)
    anchor_ = nullptr;
  else if (observed_view == highlight_border_)
    highlight_border_ = nullptr;
  else
    NOTREACHED();

  anchor_highlight_observations_.RemoveObservation(observed_view);
}

void ArcSplashScreenDialogView::OnWindowActivated(ActivationReason reason,
                                                  aura::Window* gained_active,
                                                  aura::Window* lost_active) {
  if (gained_active != parent_window())
    return;

  // Safe-guard for the activation forwarding loop.
  if (forwarding_activation_)
    return;

  forwarding_activation_ = true;
  // Forward the activation to the dialog if available.
  // To avoid nested-activation, here we post the task to the queue.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<ArcSplashScreenDialogView> view) {
                       if (!view)
                         return;

                       base::AutoReset<bool> forwarding_activation_update(
                           &view->forwarding_activation_, false);
                       auto* const widget = view->GetWidget();
                       if (!widget)
                         return;
                       if (widget->IsClosed())
                         return;
                       widget->Activate();
                     },
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcSplashScreenDialogView::OnCloseButtonClicked() {
  if (!close_callback_)
    return;

  if (anchor_ && highlight_border_)
    anchor_->RemoveChildViewT(highlight_border_.get());

  std::move(close_callback_).Run();

  auto* const widget = GetWidget();
  if (widget)
    widget->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
}

void ArcSplashScreenDialogView::Show(aura::Window* parent,
                                     bool is_for_unresizable) {
  auto* const frame_view = ash::NonClientFrameViewAsh::Get(parent);
  DCHECK(frame_view);
  auto* const anchor_view =
      frame_view->GetHeaderView()->GetFrameHeader()->GetCenterButton();

  if (!anchor_view) {
    LOG(ERROR) << "Failed to show the compat mode splash screen because the "
                  "center button is missing.";
    return;
  }

  auto dialog_view = std::make_unique<ArcSplashScreenDialogView>(
      base::BindOnce(&OverlayDialog::CloseIfAny, base::Unretained(parent)),
      parent, anchor_view, is_for_unresizable);

  OverlayDialog::Show(
      parent,
      base::BindOnce(&ArcSplashScreenDialogView::OnCloseButtonClicked,
                     dialog_view->weak_ptr_factory_.GetWeakPtr()),
      /*dialog_view=*/nullptr);

  // TODO(b/206336651): Investigate the cases when the following check fails.
  if (!anchor_view->GetWidget() ||
      !anchor_view->GetWidget()->GetNativeWindow()) {
    LOG(WARNING) << "Skipped to show the compat mode splash screen because the "
                    "anchored widget/window has already been destroyed.";
    return;
  }
  views::BubbleDialogDelegateView::CreateBubble(std::move(dialog_view))->Show();
}

BEGIN_METADATA(ArcSplashScreenDialogView)
END_METADATA

}  // namespace arc
