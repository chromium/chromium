// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_tray.h"

#include "ash/constants/tray_background_view_catalog.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/typography.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_countdown_view.h"
#include "ash/system/progress_indicator/progress_indicator.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "base/check.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

namespace {

constexpr int kIconSize = 20;
constexpr int kBubbleInset = 16;
constexpr int kTaskItemViewInsets = 6;
constexpr int kTaskItemViewCornerRadius = 16;
constexpr int kProgressIndicatorThickness = 2;
constexpr auto kTaskTitleLabelInsets = gfx::Insets::TLBR(0, 12, 0, 18);
constexpr auto kProgressIndicatorBounds = gfx::Rect(2, 0, 32, 32);
constexpr base::TimeDelta kStartAnimationDelay = base::Milliseconds(300);
constexpr base::TimeDelta kTaskItemViewFadeOutDuration =
    base::Milliseconds(200);

}  // namespace

class FocusModeTray::TaskItemView : public views::BoxLayoutView {
 public:
  TaskItemView(const std::u16string& title, PressedCallback callback) {
    SetBorder(views::CreateEmptyBorder(kTaskItemViewInsets));
    // Set the background color is not opaque.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysSystemOnBase, kTaskItemViewCornerRadius));

    radio_button_ =
        AddChildView(std::make_unique<views::ImageButton>(std::move(callback)));
    radio_button_->SetImageModel(views::Button::STATE_NORMAL,
                                 ui::ImageModel::FromVectorIcon(
                                     kRadioButtonUncheckedIcon,
                                     cros_tokens::kCrosSysPrimary, kIconSize));
    radio_button_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_FOCUS_MODE_TASK_RADIO_BUTTON));

    task_title_ = AddChildView(std::make_unique<views::Label>());
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                          *task_title_);
    task_title_->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
    task_title_->SetText(title);
    task_title_->SetTooltipText(title);
    task_title_->SetBorder(views::CreateEmptyBorder(kTaskTitleLabelInsets));
  }
  TaskItemView(const TaskItemView&) = delete;
  TaskItemView& operator=(const TaskItemView&) = delete;
  ~TaskItemView() override {
    radio_button_ = nullptr;
    task_title_ = nullptr;
  }

  const views::ImageButton* GetRadioButton() const { return radio_button_; }
  const views::Label* GetTaskTitle() const { return task_title_; }
  bool GetWasCompleted() const { return was_completed_; }

  // Sets `radio_button_` as toggled which will update the button with a check
  // icon, and adds a strike through on `task_title_`.
  void UpdateStyleToCompleted() {
    if (was_completed_) {
      return;
    }
    was_completed_ = true;

    radio_button_->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(kDoneIcon, cros_tokens::kCrosSysPrimary,
                                       kIconSize));

    task_title_->SetFontList(
        TypographyProvider::Get()
            ->ResolveTypographyToken(TypographyToken::kCrosButton2)
            .DeriveWithStyle(gfx::Font::FontStyle::STRIKE_THROUGH));
    task_title_->SetEnabledColorId(cros_tokens::kCrosSysSecondary);
  }

 private:
  bool was_completed_ = false;
  raw_ptr<views::ImageButton> radio_button_ = nullptr;
  raw_ptr<views::Label> task_title_ = nullptr;
};

FocusModeTray::FocusModeTray(Shelf* shelf)
    : TrayBackgroundView(shelf,
                         TrayBackgroundViewCatalogName::kFocusMode,
                         RoundedCornerBehavior::kAllRounded),
      image_view_(tray_container()->AddChildView(
          std::make_unique<views::ImageView>())) {
  SetCallback(base::BindRepeating(&FocusModeTray::FocusModeIconActivated,
                                  weak_ptr_factory_.GetWeakPtr()));

  image_view_->SetTooltipText(GetAccessibleNameForTray());
  image_view_->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
  image_view_->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  image_view_->SetPreferredSize(gfx::Size(kTrayItemSize, kTrayItemSize));

  tray_container()->SetPaintToLayer();
  tray_container()->layer()->SetFillsBoundsOpaquely(false);
  progress_indicator_ =
      ProgressIndicator::CreateDefaultInstance(base::BindRepeating(
          [](FocusModeTray* view) -> std::optional<float> {
            auto* controller = FocusModeController::Get();
            if (view->is_active() || !controller->in_focus_session()) {
              // `kProgressComplete` causes the layer to not be painted, hiding
              // the progress indicator.
              return ProgressIndicator::kProgressComplete;
            }
            const base::TimeDelta session_duration =
                controller->session_duration();
            const base::TimeDelta time_elapsed =
                session_duration - (controller->end_time() - base::Time::Now());
            return time_elapsed / session_duration;
          },
          base::Unretained(this)));
  progress_indicator_->SetInnerIconVisible(false);
  progress_indicator_->SetInnerRingVisible(false);
  progress_indicator_->SetOuterRingStrokeWidth(kProgressIndicatorThickness);
  progress_indicator_->SetColorId(cros_tokens::kCrosSysPrimary);

  tray_container()->layer()->Add(
      progress_indicator_->CreateLayer(base::BindRepeating(
          [](TrayContainer* view, ui::ColorId color_id) {
            return view->GetColorProvider()->GetColor(color_id);
          },
          base::Unretained(tray_container()))));
  UpdateProgressRing();
  progress_indicator_->layer()->SetBounds(kProgressIndicatorBounds);

  auto* controller = FocusModeController::Get();
  SetVisiblePreferred(controller->in_focus_session());
  controller->AddObserver(this);
}

FocusModeTray::~FocusModeTray() {
  if (bubble_) {
    bubble_->bubble_view()->ResetDelegate();
  }
  FocusModeController::Get()->RemoveObserver(this);
}

const views::ImageButton* FocusModeTray::GetRadioButtonForTesting() const {
  return task_item_view_->GetRadioButton();
}

const views::Label* FocusModeTray::GetTaskTitleForTesting() const {
  return task_item_view_->GetTaskTitle();
}

void FocusModeTray::ClickedOutsideBubble() {
  CloseBubble();
}

std::u16string FocusModeTray::GetAccessibleNameForTray() {
  // TODO(b/288975135): Update once we get UX writing.
  return l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TOGGLE_ACTIVE_LABEL);
}

void FocusModeTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {
  if (bubble_->bubble_view() == bubble_view) {
    CloseBubble();
  }
}

void FocusModeTray::HideBubble(const TrayBubbleView* bubble_view) {
  if (bubble_->bubble_view() == bubble_view) {
    CloseBubble();
  }
}

void FocusModeTray::CloseBubble() {
  if (!bubble_) {
    return;
  }

  if (auto* bubble_view = bubble_->GetBubbleView()) {
    bubble_view->ResetDelegate();
  }

  bubble_.reset();
  countdown_view_ = nullptr;
  task_item_view_ = nullptr;
  bubble_view_container_ = nullptr;
  SetIsActive(false);
  progress_indicator_->layer()->SetOpacity(1);
  UpdateProgressRing();
}

void FocusModeTray::ShowBubble() {
  if (bubble_) {
    return;
  }

  auto bubble_view =
      std::make_unique<TrayBubbleView>(CreateInitParamsForTrayBubble(
          /*tray=*/this, /*anchor_to_shelf_corner=*/false));

  bubble_view_container_ =
      bubble_view->AddChildView(std::make_unique<views::BoxLayoutView>());
  bubble_view_container_->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  bubble_view_container_->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(kBubbleInset)));
  bubble_view_container_->SetBetweenChildSpacing(kBubbleInset);

  countdown_view_ = bubble_view_container_->AddChildView(
      std::make_unique<FocusModeCountdownView>(/*include_end_button=*/true));
  countdown_view_->UpdateUI();

  const std::u16string title =
      FocusModeController::Get()->selected_task_title();
  if (!title.empty()) {
    task_item_view_ =
        bubble_view_container_->AddChildView(std::make_unique<TaskItemView>(
            title, base::BindRepeating(&FocusModeTray::OnCompleteTask,
                                       weak_ptr_factory_.GetWeakPtr())));
    task_item_view_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kPreferred));
  }

  bubble_ = std::make_unique<TrayBubbleWrapper>(this);
  bubble_->ShowBubble(std::move(bubble_view));

  SetIsActive(true);
  progress_indicator_->layer()->SetOpacity(0);
  UpdateProgressRing();
}

void FocusModeTray::UpdateTrayItemColor(bool is_active) {
  CHECK(chromeos::features::IsJellyEnabled());
  UpdateTrayIcon();
}

void FocusModeTray::OnThemeChanged() {
  TrayBackgroundView::OnThemeChanged();
  UpdateTrayIcon();
}

void FocusModeTray::OnFocusModeChanged(bool in_focus_session) {
  if (in_focus_session) {
    UpdateProgressRing();
  } else {
    CloseBubble();
  }
}

void FocusModeTray::OnTimerTick() {
  UpdateProgressRing();
  MaybeUpdateCountdownViewUI();
}

void FocusModeTray::OnSessionDurationChanged() {
  UpdateProgressRing();
  MaybeUpdateCountdownViewUI();
}

void FocusModeTray::UpdateTrayIcon() {
  SkColor color;
  if (chromeos::features::IsJellyEnabled()) {
    color = GetColorProvider()->GetColor(
        is_active() ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                    : cros_tokens::kCrosSysOnSurface);
  } else {
    color = GetColorProvider()->GetColor(kColorAshIconColorPrimary);
  }
  image_view_->SetImage(CreateVectorIcon(kFocusModeLampIcon, color));
}

void FocusModeTray::FocusModeIconActivated(const ui::Event& event) {
  if (bubble_ && bubble_->bubble_view()->GetVisible()) {
    CloseBubble();
    return;
  }

  ShowBubble();
}

void FocusModeTray::MaybeUpdateCountdownViewUI() {
  if (countdown_view_) {
    countdown_view_->UpdateUI();
  }
}

void FocusModeTray::OnCompleteTask() {
  if (!task_item_view_ || task_item_view_->GetWasCompleted()) {
    return;
  }

  task_item_view_->UpdateStyleToCompleted();

  // TODO(b/309857026): Call the task API to mark the task as completed, then
  // clean up the selected task title.
  FocusModeController::Get()->set_selected_task_title(std::u16string());

  // We want to show the check icon and a strikethrough on the label for
  // `kStartAnimationDelay` before removing `task_item_view_` from the
  // bubble.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FocusModeTray::AnimateBubbleResize,
                     weak_ptr_factory_.GetWeakPtr()),
      kStartAnimationDelay);
}

void FocusModeTray::OnBubbleResizeAnimationStarted() {
  if (bubble_) {
    auto* ptr = task_item_view_.get();
    task_item_view_ = nullptr;
    bubble_view_container_->RemoveChildViewT(ptr);
  }
}

void FocusModeTray::OnBubbleResizeAnimationEnded() {
  if (bubble_) {
    bubble_->bubble_view()->UpdateBubble();
  }
}

void FocusModeTray::AnimateBubbleResize() {
  if (!bubble_) {
    return;
  }

  // `remove_height` is the height of the `task_item_view_` and the spacing
  // above it.
  const int remove_height = task_item_view_->bounds().height() + kBubbleInset;
  auto target_bounds = bubble_->bubble_view()->layer()->bounds();
  target_bounds.Inset(gfx::Insets::TLBR(remove_height, 0, 0, 0));

  views::AnimationBuilder()
      .SetPreemptionStrategy(ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET)
      .OnStarted(base::BindOnce(&FocusModeTray::OnBubbleResizeAnimationStarted,
                                weak_ptr_factory_.GetWeakPtr()))
      .OnEnded(base::BindOnce(&FocusModeTray::OnBubbleResizeAnimationEnded,
                              weak_ptr_factory_.GetWeakPtr()))
      .Once()
      .SetDuration(kTaskItemViewFadeOutDuration)
      .SetBounds(bubble_->bubble_view()->layer(), target_bounds,
                 gfx::Tween::EASE_OUT);
}

void FocusModeTray::UpdateProgressRing() {
  // Schedule a repaint of the indicator.
  progress_indicator_->InvalidateLayer();
}

BEGIN_METADATA(FocusModeTray)
END_METADATA

}  // namespace ash
