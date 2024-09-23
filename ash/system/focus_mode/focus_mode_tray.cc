// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_tray.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/glanceables/common/glanceables_util.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/typography.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_countdown_view.h"
#include "ash/system/focus_mode/focus_mode_ending_moment_view.h"
#include "ash/system/focus_mode/focus_mode_session.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/progress_indicator/progress_indicator.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "base/check.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
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
constexpr auto kProgressIndicatorInsets = gfx::Insets(-6);
constexpr base::TimeDelta kStartAnimationDelay = base::Milliseconds(300);
constexpr base::TimeDelta kTaskItemViewFadeOutDuration =
    base::Milliseconds(200);

std::u16string GetAccessibleTrayName(
    const FocusModeSession::Snapshot& session_snapshot,
    const size_t congratulatory_index) {
  if (session_snapshot.state == FocusModeSession::State::kEnding) {
    return focus_mode_util::GetCongratulatoryTextAndEmoji(congratulatory_index);
  }

  const std::u16string duration_string =
      session_snapshot.remaining_time < base::Minutes(1)
          ? l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_FOCUS_MODE_SESSION_LESS_THAN_ONE_MINUTE)
          : focus_mode_util::GetDurationString(session_snapshot.remaining_time,
                                               /*digital_format=*/false);

  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TRAY_BUBBLE_ACCESSIBLE_NAME,
      duration_string);
}

std::u16string GetAccessibleBubbleName(
    const FocusModeSession::Snapshot& session_snapshot,
    const std::u16string& task_title,
    const size_t congratulatory_index) {
  if (session_snapshot.state == FocusModeSession::State::kEnding) {
    std::u16string title =
        focus_mode_util::GetCongratulatoryTextAndEmoji(congratulatory_index);
    std::u16string body = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_FOCUS_MODE_ENDING_MOMENT_BODY);
    return l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_FOCUS_MODE_ENDING_MOMENT_DIALOG, title, body);
  }

  const std::u16string time_remaining =
      focus_mode_util::GetDurationString(session_snapshot.remaining_time,
                                         /*digital_format=*/false);
  return task_title.empty()
             ? l10n_util::GetStringFUTF16(
                   IDS_ASH_STATUS_TRAY_FOCUS_MODE_TRAY_BUBBLE_ACCESSIBLE_NAME,
                   time_remaining)
             : l10n_util::GetStringFUTF16(
                   IDS_ASH_STATUS_TRAY_FOCUS_MODE_TRAY_BUBBLE_TASK_ACCESSIBLE_NAME,
                   time_remaining, task_title);
}

}  // namespace

class FocusModeTray::TaskItemView : public views::BoxLayoutView {
  METADATA_HEADER(TaskItemView, views::BoxLayoutView)

 public:
  TaskItemView(const std::u16string& title, PressedCallback callback) {
    SetBorder(views::CreateEmptyBorder(kTaskItemViewInsets));
    // Set the background color is not opaque.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysSystemOnBase, kTaskItemViewCornerRadius));

    const bool is_network_connected = glanceables_util::IsNetworkConnected();
    radio_button_ =
        AddChildView(std::make_unique<views::ImageButton>(std::move(callback)));
    radio_button_->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(kRadioButtonUncheckedIcon,
                                       is_network_connected
                                           ? cros_tokens::kCrosSysPrimary
                                           : cros_tokens::kCrosSysDisabled,
                                       kIconSize));

    const std::u16string radio_text = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_FOCUS_MODE_TASK_VIEW_RADIO_BUTTON);
    views::ViewAccessibility& radio_button_view_a11y =
        radio_button_->GetViewAccessibility();
    radio_button_view_a11y.SetName(radio_text);
    radio_button_view_a11y.SetDescription(title);
    radio_button_->SetTooltipText(radio_text);
    radio_button_->SetEnabled(is_network_connected);

    task_title_ = AddChildView(std::make_unique<views::Label>());
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                          *task_title_);
    task_title_->SetEnabledColorId(is_network_connected
                                       ? cros_tokens::kCrosSysOnSurface
                                       : cros_tokens::kCrosSysDisabled);
    task_title_->SetText(title);
    task_title_->SetTooltipText(title);
    task_title_->SetBorder(views::CreateEmptyBorder(kTaskTitleLabelInsets));
    task_title_->SetEnabled(is_network_connected);
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

  void UpdateTitle(const std::u16string& title) {
    radio_button_->GetViewAccessibility().SetDescription(title);
    task_title_->SetText(title);
    task_title_->SetTooltipText(title);
  }

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

BEGIN_METADATA(FocusModeTray, TaskItemView)
END_METADATA

FocusModeTray::FocusModeTray(Shelf* shelf)
    : TrayBackgroundView(shelf,
                         TrayBackgroundViewCatalogName::kFocusMode,
                         RoundedCornerBehavior::kAllRounded),
      image_view_(tray_container()->AddChildView(
          std::make_unique<views::ImageView>())) {
  SetCallback(base::BindRepeating(&FocusModeTray::FocusModeIconActivated,
                                  weak_ptr_factory_.GetWeakPtr()));

  image_view_->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
  image_view_->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  image_view_->SetPreferredSize(gfx::Size(kTrayItemSize, kTrayItemSize));

  tray_container()->SetPaintToLayer();
  tray_container()->layer()->SetFillsBoundsOpaquely(false);
  progress_indicator_ =
      ProgressIndicator::CreateDefaultInstance(base::BindRepeating(
          [](FocusModeTray* view) -> std::optional<float> {
            if (!view->visible_preferred() || view->is_active()) {
              return 0.0f;
            }
            if (view->show_progress_ring_after_animation_) {
              return ProgressIndicator::kForcedShow;
            }

            auto* controller = FocusModeController::Get();

            // `kProgressComplete` is only returned by an ending moment, so that
            // we can know when the pulse animation is done.
            if (controller->in_ending_moment()) {
              if (!view->bounce_in_animation_finished_) {
                return ProgressIndicator::kForcedShow;
              }

              bool is_animating = false;
              if (auto* progress_ring_animation =
                      view->progress_indicator_->animation_registry()
                          ->GetProgressRingAnimationForKey(
                              view->progress_indicator_->animation_key())) {
                is_animating = progress_ring_animation->IsAnimating();
              }

              // After the pulse animation, the ring isn't shown when the value
              // is left at `kProgressComplete`, so we need to set it manually
              // to `kForcedShow` to show the ring.
              if (!is_animating && (view->progress_indicator_->progress() ==
                                    ProgressIndicator::kProgressComplete)) {
                view->show_progress_ring_after_animation_ = true;
                return ProgressIndicator::kForcedShow;
              }
              return ProgressIndicator::kProgressComplete;
            }

            return controller->current_session()
                ->GetSnapshot(base::Time::Now())
                .progress;
          },
          base::Unretained(this)));
  progress_indicator_->SetInnerIconVisible(false);
  progress_indicator_->SetInnerRingVisible(false);
  progress_indicator_->SetOuterRingStrokeWidth(kProgressIndicatorThickness);
  progress_indicator_->SetColorId(cros_tokens::kCrosRefPrimary70);

  tray_container()->layer()->Add(
      progress_indicator_->CreateLayer(base::BindRepeating(
          [](TrayContainer* view, ui::ColorId color_id) {
            return view->GetColorProvider()->GetColor(color_id);
          },
          base::Unretained(tray_container()))));
  UpdateProgressRing();

  auto* controller = FocusModeController::Get();
  SetVisiblePreferred(controller->in_focus_session() ||
                      controller->in_ending_moment());
  tasks_observation_.Observe(&controller->tasks_model());
  controller->AddObserver(this);
}

FocusModeTray::~FocusModeTray() {
  if (bubble_) {
    bubble_->bubble_view()->ResetDelegate();
  }
  tasks_observation_.Reset();
  FocusModeController::Get()->RemoveObserver(this);
}

void FocusModeTray::ClickedOutsideBubble(const ui::LocatedEvent& event) {
  Shelf* target_shelf =
      Shelf::ForWindow(static_cast<aura::Window*>(event.target()));
  auto* target_tray = target_shelf->status_area_widget()->focus_mode_tray();
  // Do not reset the focus session if the located event is on a different
  // `FocusModeTray` view.
  if (shelf() != target_shelf && target_tray->EventTargetsTray(event)) {
    CloseBubbleAndMaybeReset(/*should_reset=*/false);
    return;
  }

  CloseBubble();
}

std::u16string FocusModeTray::GetAccessibleNameForTray() {
  if (!session_snapshot_) {
    return std::u16string();
  }

  return GetAccessibleTrayName(
      session_snapshot_.value(),
      FocusModeController::Get()->congratulatory_index());
}

std::u16string FocusModeTray::GetAccessibleNameForBubble() {
  if (!session_snapshot_) {
    return std::u16string();
  }

  auto* focus_mode_controller = FocusModeController::Get();
  const FocusModeTask* selected_task =
      focus_mode_controller->tasks_model().selected_task();
  const std::u16string task_title =
      selected_task ? base::UTF8ToUTF16(selected_task->title)
                    : std::u16string();

  return GetAccessibleBubbleName(session_snapshot_.value(), task_title,
                                 focus_mode_controller->congratulatory_index());
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

TrayBubbleView* FocusModeTray::GetBubbleView() {
  return bubble_ ? bubble_->bubble_view() : nullptr;
}

void FocusModeTray::CloseBubbleInternal() {
  CloseBubbleAndMaybeReset(/*should_reset=*/true);
}

void FocusModeTray::ShowBubble() {
  if (bubble_) {
    return;
  }

  auto* controller = FocusModeController::Get();
  CHECK(controller->current_session());

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
  ending_moment_view_ = bubble_view_container_->AddChildView(
      std::make_unique<FocusModeEndingMomentView>());

  session_snapshot_ =
      controller->current_session()->GetSnapshot(base::Time::Now());
  UpdateBubbleViews(session_snapshot_.value());

  bubble_ = std::make_unique<TrayBubbleWrapper>(this);
  bubble_->ShowBubble(std::move(bubble_view));

  SetIsActive(true);
  progress_indicator_->layer()->SetOpacity(0);
  UpdateProgressRing();

  controller->tasks_model().RequestUpdate();

  if (session_snapshot_->state == FocusModeSession::State::kEnding) {
    controller->OnEndingBubbleShown();
    AnchoredNudgeManager::Get()->MaybeRecordNudgeAction(
        NudgeCatalogName::kFocusModeEndingMomentNudge);
  }
}

void FocusModeTray::UpdateTrayItemColor(bool is_active) {
  UpdateTrayIcon();
}

void FocusModeTray::OnThemeChanged() {
  TrayBackgroundView::OnThemeChanged();
  UpdateTrayIcon();
}

void FocusModeTray::OnAnimationEnded() {
  TrayBackgroundView::OnAnimationEnded();

  // The bounce-in animation can happen on a session start or on an ending
  // moment start. Only for the bounce-in animation during the ending moment, we
  // will set `bounce_in_animation_finished_` to tell the progress callback the
  // animation was ended.
  auto* controller = FocusModeController::Get();
  if (!visible_preferred() || !controller->in_ending_moment()) {
    return;
  }

  bounce_in_animation_finished_ = true;
  controller->MaybeShowEndingMomentNudge();
}

void FocusModeTray::OnFocusModeChanged(FocusModeSession::State session_state) {
  UpdateProgressRing();
  show_progress_ring_after_animation_ = false;
  progress_ring_update_threshold_ = 0.0;

  auto* focus_mode_controller = FocusModeController::Get();
  auto current_session = focus_mode_controller->current_session();
  if (!current_session) {
    session_snapshot_.reset();
    return;
  }

  session_snapshot_ = current_session->GetSnapshot(base::Time::Now());
  image_view_->SetTooltipText(
      GetAccessibleTrayName(session_snapshot_.value(),
                            focus_mode_controller->congratulatory_index()));

  if (bubble_) {
    UpdateBubbleViews(session_snapshot_.value());
  } else if (session_snapshot_->state == FocusModeSession::State::kEnding) {
    bounce_in_animation_finished_ = false;
    MaybePlayBounceInAnimation();
  }
}

void FocusModeTray::OnTimerTick(
    const FocusModeSession::Snapshot& session_snapshot) {
  session_snapshot_ = session_snapshot;
  image_view_->SetTooltipText(GetAccessibleTrayName(
      session_snapshot_.value(),
      FocusModeController::Get()->congratulatory_index()));

  // We only paint the progress ring if it has reached the next threshold of
  // progress. This is to try and decrease power usage of Focus mode when the
  // user is idling and there are no required paints in the display.
  if (session_snapshot_->progress >= progress_ring_update_threshold_) {
    UpdateProgressRing();
    // Change the next progress step into a percentage threshold.
    progress_ring_update_threshold_ =
        (double)focus_mode_util::GetNextProgressStep(
            session_snapshot_->progress) /
        focus_mode_util::kProgressIndicatorSteps;
  }
  MaybeUpdateCountdownViewUI(session_snapshot);
}

void FocusModeTray::OnActiveSessionDurationChanged(
    const FocusModeSession::Snapshot& session_snapshot) {
  session_snapshot_ = session_snapshot;
  image_view_->SetTooltipText(GetAccessibleTrayName(
      session_snapshot_.value(),
      FocusModeController::Get()->congratulatory_index()));
  UpdateProgressRing();
  progress_ring_update_threshold_ = 0.0;
  MaybeUpdateCountdownViewUI(session_snapshot);
}

void FocusModeTray::OnSelectedTaskChanged(
    const std::optional<FocusModeTask>& task) {
  if (!bubble_) {
    return;
  }

  // Task was either completed or cleared.
  if (!task) {
    selected_task_.reset();
    if (!task_item_view_) {
      // Task view is already gone. Nothing to do.
      return;
    }

    if (task_item_view_->GetWasCompleted()) {
      // Task was already completed and is in the process of being deleted.
      return;
    }

    // Task was deleted.
    OnClearTask();
    return;
  }

  // A new task was picked or updated. Update the UI.
  const std::string& task_title = task->title;
  if (task_title.empty()) {
    // Can't create a task view for an empty title.
    return;
  }

  selected_task_ = task->task_id;

  if (task_item_view_) {
    // Assume that the title changed and try to update it.
    task_item_view_->UpdateTitle(base::UTF8ToUTF16(task_title));
    return;
  }

  CreateTaskItemView(task_title);

  // We need to update the bubble after creating the `task_item_view_` so the
  // widget bounds are updated and shows the view.
  bubble_->bubble_view()->UpdateBubble();
}

void FocusModeTray::OnTasksUpdated(const std::vector<FocusModeTask>& tasks) {}

void FocusModeTray::OnTaskCompleted(const FocusModeTask& completed_task) {
  // Initiate UI update to indicate that the task was completed.
  if (!task_item_view_ || task_item_view_->GetWasCompleted()) {
    return;
  }

  task_item_view_->UpdateStyleToCompleted();

  OnClearTask();
}

void FocusModeTray::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);

  // Position the progress indicator based on the position of the image view.
  // The centered position inside of the tray container changes based on shelf
  // orientation and tablet mode, but there is already logic to keep the image
  // view centered that we can use.
  gfx::Rect progress_bounds = gfx::Rect(views::View::ConvertRectToTarget(
      /*source=*/image_view_,
      /*target=*/tray_container(), image_view_->GetImageBounds()));
  progress_bounds.Inset(kProgressIndicatorInsets);
  progress_indicator_->layer()->SetBounds(progress_bounds);
}

void FocusModeTray::MaybePlayBounceInAnimation() {
  if (bubble_ || !FocusModeController::Get()->in_ending_moment()) {
    return;
  }

  BounceInAnimation(/*scale_animation=*/false);
}

const views::ImageButton* FocusModeTray::GetRadioButtonForTesting() const {
  return task_item_view_->GetRadioButton();
}

const views::Label* FocusModeTray::GetTaskTitleForTesting() const {
  return task_item_view_->GetTaskTitle();
}

void FocusModeTray::CreateTaskItemView(const std::string& task_title) {
  if (task_title.empty()) {
    return;
  }

  task_item_view_ =
      bubble_view_container_->AddChildView(std::make_unique<TaskItemView>(
          base::UTF8ToUTF16(task_title),
          base::BindRepeating(&FocusModeTray::HandleCompleteTaskButton,
                              weak_ptr_factory_.GetWeakPtr())));
  task_item_view_->SetProperty(views::kBoxLayoutFlexKey,
                               views::BoxLayoutFlexSpecification());
}

void FocusModeTray::UpdateTrayIcon() {
  SkColor color = GetColorProvider()->GetColor(
      is_active() ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                  : cros_tokens::kCrosSysOnSurface);
  image_view_->SetImage(CreateVectorIcon(kFocusModeLampIcon, color));
}

void FocusModeTray::FocusModeIconActivated(const ui::Event& event) {
  if (bubble_ && bubble_->bubble_view()->GetVisible()) {
    CloseBubble();
    return;
  }

  ShowBubble();
}

void FocusModeTray::UpdateBubbleViews(
    const FocusModeSession::Snapshot& session_snapshot) {
  const bool is_ending_moment =
      session_snapshot.state == FocusModeSession::State::kEnding;
  countdown_view_->SetVisible(!is_ending_moment);
  ending_moment_view_->SetVisible(is_ending_moment);
  if (is_ending_moment) {
    MaybeUpdateEndingMomentViewUI(session_snapshot);
  } else {
    MaybeUpdateCountdownViewUI(session_snapshot);
  }
}

void FocusModeTray::MaybeUpdateCountdownViewUI(
    const FocusModeSession::Snapshot& session_snapshot) {
  if (countdown_view_ && countdown_view_->GetVisible() &&
      session_snapshot.state == FocusModeSession::State::kOn) {
    countdown_view_->UpdateUI(session_snapshot);
  }
}

void FocusModeTray::MaybeUpdateEndingMomentViewUI(
    const FocusModeSession::Snapshot& session_snapshot) {
  if (ending_moment_view_ && ending_moment_view_->GetVisible()) {
    ending_moment_view_->ShowEndingMomentContents(
        FocusModeController::CanExtendSessionDuration(session_snapshot));
  }
}

void FocusModeTray::HandleCompleteTaskButton() {
  // The user clicked on the task complete button. Notify the model. UI updates
  // happen in the model events.
  if (!selected_task_.has_value()) {
    // If there is no selected id, `OnClearTask()` should have been triggered
    // already either by `OnTaskCompleted()` or `OnSelectedTaskChanged()`, so
    // we can just return.
    return;
  }

  FocusModeController::Get()->tasks_model().UpdateTask(
      FocusModeTasksModel::TaskUpdate::CompletedUpdate(*selected_task_));
}

void FocusModeTray::OnClearTask() {
  if (!selected_task_.has_value()) {
    return;
  }

  selected_task_.reset();
  if (!task_item_view_) {
    return;
  }

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
  if (bubble_ && task_item_view_) {
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
  // If there is no `task_item_view_` or it has already been cleared, we should
  // skip the animation.
  if (!bubble_ || !task_item_view_) {
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

bool FocusModeTray::EventTargetsTray(const ui::LocatedEvent& event) const {
  if (event.target() != GetWidget()->GetNativeWindow()) {
    return false;
  }

  gfx::Point location_in_status_area = event.location();
  views::View::ConvertPointFromWidget(this, &location_in_status_area);
  return bounds().Contains(location_in_status_area);
}

void FocusModeTray::CloseBubbleAndMaybeReset(bool should_reset) {
  if (!bubble_) {
    return;
  }

  if (auto* bubble_view = bubble_->GetBubbleView()) {
    bubble_view->ResetDelegate();
  }

  bubble_.reset();
  countdown_view_ = nullptr;
  ending_moment_view_ = nullptr;
  task_item_view_ = nullptr;
  bubble_view_container_ = nullptr;
  SetIsActive(false);
  progress_indicator_->layer()->SetOpacity(1);
  UpdateProgressRing();

  if (auto* controller = FocusModeController::Get();
      !controller->in_focus_session() && should_reset) {
    controller->ResetFocusSession();
  }
}

BEGIN_METADATA(FocusModeTray)
END_METADATA

}  // namespace ash
