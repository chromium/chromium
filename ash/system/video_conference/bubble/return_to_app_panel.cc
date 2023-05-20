// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/return_to_app_panel.h"

#include <memory>
#include <string>

#include "ash/bubble/bubble_utils.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash::video_conference {

namespace {

const int kReturnToAppPanelRadius = 16;
const int kReturnToAppPanelExpandedTopPadding = 12;
const int kReturnToAppPanelVerticalPadding = 8;
const int kReturnToAppPanelSidePadding = 16;
const int kReturnToAppPanelSpacing = 8;
const int kReturnToAppButtonTopRowSpacing = 12;
const int kReturnToAppButtonSpacing = 16;
const int kReturnToAppButtonIconsSpacing = 2;
const int kReturnToAppIconSize = 20;

constexpr auto kPanelBoundsChangeAnimationDuration = base::Milliseconds(200);

void StartReportLayerAnimationSmoothness(
    const std::string& animation_histogram_name,
    int smoothness) {
  if (animation_histogram_name.empty()) {
    return;
  }
  base::UmaHistogramPercentage(animation_histogram_name, smoothness);
}

void StartRecordAnimationSmoothness(
    views::Widget* widget,
    absl::optional<ui::ThroughputTracker>& tracker) {
  // `widget` may not exist in tests.
  if (!widget) {
    return;
  }

  tracker.emplace(widget->GetCompositor()->RequestNewThroughputTracker());
  tracker->Start(
      ash::metrics_util::ForSmoothness(base::BindRepeating([](int smoothness) {
        base::UmaHistogramPercentage(
            "Ash.VideoConference.ReturnToAppPanel.BoundsChange."
            "AnimationSmoothness",
            smoothness);
      })));
}

// Performs fade in/fade out animation using `AnimationBuilder`.
void FadeInView(views::View* view,
                int delay_in_ms,
                int duration_in_ms,
                const std::string& animation_histogram_name) {
  // If we are in testing with animation (non zero duration), we shouldn't have
  // delays so that we can properly track when animation is completed in test.
  if (ui::ScopedAnimationDurationScaleMode::duration_multiplier() ==
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION) {
    delay_in_ms = 0;
  }

  // The view must have a layer to perform animation.
  CHECK(view->layer());

  ui::AnimationThroughputReporter reporter(
      view->layer()->GetAnimator(),
      metrics_util::ForSmoothness(base::BindRepeating(
          &StartReportLayerAnimationSmoothness, animation_histogram_name)));

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(base::TimeDelta())
      .SetOpacity(view, 0.0f)
      .At(base::Milliseconds(delay_in_ms))
      .SetDuration(base::Milliseconds(duration_in_ms))
      .SetOpacity(view, 1.0f);
}

void FadeOutView(views::View* view,
                 base::WeakPtr<ReturnToAppPanel> parent_weak_ptr,
                 const std::string& animation_histogram_name) {
  auto on_animation_ended = base::BindOnce(
      [](base::WeakPtr<ReturnToAppPanel> parent_weak_ptr, views::View* view) {
        if (parent_weak_ptr) {
          view->layer()->SetOpacity(1.0f);
          view->SetVisible(false);
        }
      },
      parent_weak_ptr, view);

  std::pair<base::OnceClosure, base::OnceClosure> split =
      base::SplitOnceCallback(std::move(on_animation_ended));

  // The view must have a layer to perform animation.
  CHECK(view->layer());

  ui::AnimationThroughputReporter reporter(
      view->layer()->GetAnimator(),
      metrics_util::ForSmoothness(base::BindRepeating(
          &StartReportLayerAnimationSmoothness, animation_histogram_name)));

  view->SetVisible(true);
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(std::move(split.first))
      .OnAborted(std::move(split.second))
      .Once()
      .SetDuration(base::Milliseconds(50))
      .SetVisibility(view, false)
      .SetOpacity(view, 0.0f);
}

// Creates a view containing camera, microphone, and screen share icons that
// shows capturing state of a media app.
std::unique_ptr<views::View> CreateReturnToAppIconsContainer(
    bool is_capturing_camera,
    bool is_capturing_microphone,
    bool is_capturing_screen) {
  auto container = std::make_unique<views::View>();
  container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(0, kReturnToAppButtonIconsSpacing / 2, 0,
                                    kReturnToAppButtonIconsSpacing / 2));

  if (is_capturing_camera) {
    auto camera_icon = std::make_unique<views::ImageView>();
    camera_icon->SetImage(ui::ImageModel::FromVectorIcon(
        kPrivacyIndicatorsCameraIcon, cros_tokens::kCrosSysPositive,
        kReturnToAppIconSize));
    container->AddChildView(std::move(camera_icon));
  }

  if (is_capturing_microphone) {
    auto microphone_icon = std::make_unique<views::ImageView>();
    microphone_icon->SetImage(ui::ImageModel::FromVectorIcon(
        kPrivacyIndicatorsMicrophoneIcon, cros_tokens::kCrosSysPositive,
        kReturnToAppIconSize));
    container->AddChildView(std::move(microphone_icon));
  }

  if (is_capturing_screen) {
    auto screen_share_icon = std::make_unique<views::ImageView>();
    screen_share_icon->SetImage(ui::ImageModel::FromVectorIcon(
        kPrivacyIndicatorsScreenShareIcon, cros_tokens::kCrosSysPositive,
        kReturnToAppIconSize));
    container->AddChildView(std::move(screen_share_icon));
  }

  return container;
}

// Gets the display text representing a media app shown in the return to app
// panel.
std::u16string GetMediaAppDisplayText(
    mojo::StructPtr<crosapi::mojom::VideoConferenceMediaAppInfo>& media_app) {
  auto url = media_app->url;
  auto title = media_app->title;

  // Displays the title if it is not empty. Otherwise, display app url.
  if (!title.empty()) {
    return title;
  }

  if (url) {
    return base::UTF8ToUTF16(url->GetContent());
  }

  return std::u16string();
}

// A customized toggle button for the return to app panel, which rotates
// depending on the expand state.
class ReturnToAppExpandButton : public views::ImageView,
                                ReturnToAppButton::Observer {
 public:
  explicit ReturnToAppExpandButton(ReturnToAppButton* return_to_app_button)
      : return_to_app_button_(return_to_app_button) {
    return_to_app_button_->AddObserver(this);
  }

  ReturnToAppExpandButton(const ReturnToAppExpandButton&) = delete;
  ReturnToAppExpandButton& operator=(const ReturnToAppExpandButton&) = delete;

  ~ReturnToAppExpandButton() override {
    return_to_app_button_->RemoveObserver(this);
  }

  // views::ImageView:
  void OnPaint(gfx::Canvas* canvas) override {
    // Rotate the canvas to rotate the button depending on the panel's expanded
    // state.
    gfx::ScopedCanvas scoped(canvas);
    canvas->Translate(gfx::Vector2d(size().width() / 2, size().height() / 2));
    if (!expanded_) {
      canvas->sk_canvas()->rotate(180.);
    }
    gfx::ImageSkia image = GetImage();
    canvas->DrawImageInt(image, -image.width() / 2, -image.height() / 2);
  }

 private:
  // ReturnToAppButton::Observer:
  void OnExpandedStateChanged(bool expanded) override {
    if (expanded_ == expanded) {
      return;
    }
    expanded_ = expanded;

    // Repaint to rotate the button.
    SchedulePaint();
  }

  // Indicates if this button (and also the parent panel) is in the expanded
  // state.
  bool expanded_ = false;

  // Owned by the views hierarchy. Will be destroyed after this view since it is
  // the parent.
  const raw_ptr<ReturnToAppButton, ExperimentalAsh> return_to_app_button_;
};

}  // namespace

// -----------------------------------------------------------------------------
// ReturnToAppButton:

ReturnToAppButton::ReturnToAppButton(
    ReturnToAppPanel* panel,
    bool is_top_row,
    const base::UnguessableToken& id,
    bool is_capturing_camera,
    bool is_capturing_microphone,
    bool is_capturing_screen,
    const std::u16string& display_text,
    crosapi::mojom::VideoConferenceAppType app_type)
    : is_capturing_camera_(is_capturing_camera),
      is_capturing_microphone_(is_capturing_microphone),
      is_capturing_screen_(is_capturing_screen),
      panel_(panel) {
  SetCallback(base::BindRepeating(&ReturnToAppButton::OnButtonClicked,
                                  weak_ptr_factory_.GetWeakPtr(), id,
                                  app_type));

  auto spacing = is_top_row ? kReturnToAppButtonTopRowSpacing / 2
                            : kReturnToAppButtonSpacing / 2;
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(is_top_row ? views::LayoutAlignment::kCenter
                                       : views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetDefault(views::kMarginsKey, gfx::Insets::TLBR(0, spacing, 0, spacing))
      .SetInteriorMargin(gfx::Insets::TLBR(0, kReturnToAppPanelSidePadding, 0,
                                           kReturnToAppPanelSidePadding));

  icons_container_ = AddChildView(CreateReturnToAppIconsContainer(
      is_capturing_camera, is_capturing_microphone, is_capturing_screen));
  if (!is_top_row) {
    icons_container_->SetPreferredSize(
        gfx::Size(/*width=*/kReturnToAppIconSize * panel->max_capturing_count(),
                  /*height=*/kReturnToAppIconSize));
  }

  auto label = std::make_unique<views::Label>(display_text);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred));

  label->SetAutoColorReadabilityEnabled(false);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kLegacyBody2, *label);
  label->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);

  label_ = AddChildView(std::move(label));

  if (is_top_row) {
    auto expand_indicator = std::make_unique<ReturnToAppExpandButton>(this);
    expand_indicator->SetImage(ui::ImageModel::FromVectorIcon(
        kUnifiedMenuExpandIcon, cros_tokens::kCrosSysSecondary, 16));
    expand_indicator->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_ASH_VIDEO_CONFERENCE_RETURN_TO_APP_SHOW_TOOLTIP));
    expand_indicator_ = AddChildView(std::move(expand_indicator));

    // Add a layer for icons container in the top row to perform animation.
    icons_container_->SetPaintToLayer();
    icons_container_->layer()->SetFillsBoundsOpaquely(false);
  }

  // An empty `id` means that this view is not associated with any particular
  // app and it is the summary row, so we will just use the `display_text` as
  // accessible name in this case rather than the full string.
  std::u16string return_to_app_accessible_name =
      id.is_empty()
          ? display_text
          : l10n_util::GetStringFUTF16(
                VIDEO_CONFERENCE_RETURN_TO_APP_ACCESSIBLE_NAME, display_text);

  SetAccessibleName(GetPeripheralsAccessibleName() +
                    return_to_app_accessible_name);

  // When we show the bubble for the first time, only the top row is visible.
  SetVisible(is_top_row);

  if (!is_top_row) {
    // Add a layer to perform fade in animation.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
  }
}

ReturnToAppButton::~ReturnToAppButton() = default;

void ReturnToAppButton::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ReturnToAppButton::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ReturnToAppButton::OnButtonClicked(
    const base::UnguessableToken& id,
    crosapi::mojom::VideoConferenceAppType app_type) {
  // For rows that are not the summary row (which has non-empty `id`), perform
  // return to app.
  if (!id.is_empty()) {
    ash::VideoConferenceTrayController::Get()->ReturnToApp(id);
    base::UmaHistogramEnumeration("Ash.VideoConference.ReturnToApp.Click",
                                  app_type);
    return;
  }

  // If the expand/collapse animation is running, we should not toggle the state
  // (to avoid spam clicking this button and snapping the animation).
  if (panel_->IsExpandCollapseAnimationRunning()) {
    return;
  }

  // For summary row, toggle the expand state.
  expanded_ = !expanded_;

  for (auto& observer : observer_list_) {
    observer.OnExpandedStateChanged(expanded_);
  }

  icons_container_->SetVisible(!expanded_);
  auto tooltip_text_id =
      expanded_ ? IDS_ASH_VIDEO_CONFERENCE_RETURN_TO_APP_HIDE_TOOLTIP
                : IDS_ASH_VIDEO_CONFERENCE_RETURN_TO_APP_SHOW_TOOLTIP;
  expand_indicator_->SetTooltipText(l10n_util::GetStringUTF16(tooltip_text_id));

  if (icons_container_->GetVisible()) {
    FadeInView(icons_container_, /*delay_in_ms=*/100, /*duration_in_ms=*/100,
               /*animation_histogram_name=*/
               "Ash.VideoConference.SummaryIcons.FadeIn.AnimationSmoothness");
  }
}

std::u16string ReturnToAppButton::GetPeripheralsAccessibleName() {
  std::u16string tooltip_text;
  if (is_capturing_camera_) {
    tooltip_text += l10n_util::GetStringFUTF16(
        VIDEO_CONFERENCE_RETURN_TO_APP_PERIPHERALS_ACCESSIBLE_NAME,
        l10n_util::GetStringUTF16(VIDEO_CONFERENCE_TOGGLE_BUTTON_TYPE_CAMERA));
  }
  if (is_capturing_microphone_) {
    tooltip_text += l10n_util::GetStringFUTF16(
        VIDEO_CONFERENCE_RETURN_TO_APP_PERIPHERALS_ACCESSIBLE_NAME,
        l10n_util::GetStringUTF16(
            VIDEO_CONFERENCE_TOGGLE_BUTTON_TYPE_MICROPHONE));
  }
  if (is_capturing_screen_) {
    tooltip_text += l10n_util::GetStringFUTF16(
        VIDEO_CONFERENCE_RETURN_TO_APP_PERIPHERALS_ACCESSIBLE_NAME,
        l10n_util::GetStringUTF16(
            VIDEO_CONFERENCE_TOGGLE_BUTTON_TYPE_SCREEN_SHARE));
  }
  return tooltip_text;
}

// -----------------------------------------------------------------------------
// ReturnToAppContainer:

ReturnToAppPanel::ReturnToAppContainer::ReturnToAppContainer()
    : views::AnimationDelegateViews(this),
      animation_(std::make_unique<gfx::LinearAnimation>(
          kPanelBoundsChangeAnimationDuration,
          gfx::LinearAnimation::kDefaultFrameRate,
          /*delegate=*/this)) {
  auto flex_layout = std::make_unique<views::FlexLayout>();
  flex_layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(0, 0, kReturnToAppPanelSpacing, 0));
  layout_manager_ = SetLayoutManager(std::move(flex_layout));
  AdjustLayoutForExpandCollapseState(/*expanded=*/false);

  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBase, kReturnToAppPanelRadius));
}

ReturnToAppPanel::ReturnToAppContainer::~ReturnToAppContainer() = default;

void ReturnToAppPanel::ReturnToAppContainer::StartExpandCollapseAnimation() {
  // Animation should be guarded not to perform in `ReturnToAppButton` if
  // there's a current running animation.
  CHECK(!animation_->is_animating());

  animation_->Start();
  StartRecordAnimationSmoothness(GetWidget(), throughput_tracker_);
}

void ReturnToAppPanel::ReturnToAppContainer::AdjustLayoutForExpandCollapseState(
    bool expanded) {
  layout_manager_->SetInteriorMargin(
      gfx::Insets::TLBR(expanded ? kReturnToAppPanelExpandedTopPadding
                                 : kReturnToAppPanelVerticalPadding,
                        0, kReturnToAppPanelVerticalPadding, 0));
}

void ReturnToAppPanel::ReturnToAppContainer::AnimationProgressed(
    const gfx::Animation* animation) {
  PreferredSizeChanged();
}

void ReturnToAppPanel::ReturnToAppContainer::AnimationEnded(
    const gfx::Animation* animation) {
  PreferredSizeChanged();

  if (throughput_tracker_) {
    // Reset `throughput_tracker_` to record animation smoothness.
    throughput_tracker_->Stop();
    throughput_tracker_.reset();
  }
}

void ReturnToAppPanel::ReturnToAppContainer::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

gfx::Size ReturnToAppPanel::ReturnToAppContainer::CalculatePreferredSize()
    const {
  gfx::Size size = views::View::CalculatePreferredSize();

  if (!animation_->is_animating()) {
    return size;
  }

  auto tween_type = expanded_target_ ? gfx::Tween::ACCEL_20_DECEL_100
                                     : gfx::Tween::ACCEL_40_DECEL_100_3;

  // The height will be determined by adding the extra height with the previous
  // height of the container before the animation starts. The extra height will
  // be a positive value when the panel is expanding, and negative if the panel
  // is collapsing.
  double extra_height =
      (size.height() - height_before_animation_) *
      gfx::Tween::CalculateValue(tween_type, animation_->GetCurrentValue());

  size.set_height(height_before_animation_ + extra_height);
  return size;
}

// -----------------------------------------------------------------------------
// ReturnToAppPanel:

ReturnToAppPanel::ReturnToAppPanel() {
  SetID(BubbleViewID::kReturnToApp);

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetInteriorMargin(gfx::Insets::TLBR(16, 16, 0, 16));

  auto container_view = std::make_unique<ReturnToAppContainer>();
  container_view_ = AddChildView(std::move(container_view));

  // Add running media apps buttons to the panel.
  VideoConferenceTrayController::Get()->GetMediaApps(base::BindOnce(
      &ReturnToAppPanel::AddButtonsToPanel, weak_ptr_factory_.GetWeakPtr()));
}

ReturnToAppPanel::~ReturnToAppPanel() {
  // We only need to remove observer in case that there's a summary row
  // (multiple apps).
  if (summary_row_view_) {
    summary_row_view_->RemoveObserver(this);
  }
}

bool ReturnToAppPanel::IsExpandCollapseAnimationRunning() {
  return container_view_->animation()->is_animating();
}

void ReturnToAppPanel::OnExpandedStateChanged(bool expanded) {
  container_view_->set_height_before_animation(
      container_view_->GetPreferredSize().height());
  container_view_->AdjustLayoutForExpandCollapseState(expanded);

  for (auto* child : container_view_->children()) {
    // Skip the first child since we always show the summary row. Otherwise,
    // show the other rows if `expanded` and vice versa.
    if (child == container_view_->children().front()) {
      continue;
    }
    child->SetVisible(expanded);

    if (expanded) {
      FadeInView(
          child, /*delay_in_ms=*/50,
          /*duration_in_ms=*/150, /*animation_histogram_name=*/
          "Ash.VideoConference.ReturnToAppButton.FadeIn.AnimationSmoothness");
    } else {
      FadeOutView(
          child, weak_ptr_factory_.GetWeakPtr(), /*animation_histogram_name=*/
          "Ash.VideoConference.ReturnToAppButton.FadeOut.AnimationSmoothness");
    }
  }

  // In tests, widget might be null and the animation, in some cases, might be
  // configured to have zero duration.
  if (GetWidget() &&
      ui::ScopedAnimationDurationScaleMode::duration_multiplier() !=
          ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {
    container_view_->set_expanded_target(expanded);
    container_view_->StartExpandCollapseAnimation();
  } else {
    PreferredSizeChanged();
  }
}

void ReturnToAppPanel::ChildPreferredSizeChanged(View* child) {
  PreferredSizeChanged();
}

void ReturnToAppPanel::AddButtonsToPanel(MediaApps apps) {
  // Every exit path of this function needs to call `PreferredSizeChanged()`
  // because these views are added asynchronously in LaCrOS, after the bubble
  // widget has allocated size for the bubble. See b/273814401.
  base::ScopedClosureRunner preferred_size_changed_runner(base::BindOnce(
      &ReturnToAppPanel::PreferredSizeChanged, base::Unretained(this)));

  if (apps.size() < 1) {
    SetVisible(false);
    return;
  }

  if (apps.size() == 1) {
    auto& app = apps.front();
    auto app_button = std::make_unique<ReturnToAppButton>(
        /*panel=*/this,
        /*is_top_row=*/true, app->id, app->is_capturing_camera,
        app->is_capturing_microphone, app->is_capturing_screen,
        GetMediaAppDisplayText(app), app->app_type);
    app_button->expand_indicator()->SetVisible(false);
    container_view_->AddChildView(std::move(app_button));
    return;
  }

  bool any_apps_capturing_camera = false;
  bool any_apps_capturing_microphone = false;
  bool any_apps_capturing_screen = false;

  for (auto& app : apps) {
    max_capturing_count_ =
        std::max(max_capturing_count_, app->is_capturing_camera +
                                           app->is_capturing_microphone +
                                           app->is_capturing_screen);

    any_apps_capturing_camera |= app->is_capturing_camera;
    any_apps_capturing_microphone |= app->is_capturing_microphone;
    any_apps_capturing_screen |= app->is_capturing_screen;
  }

  auto summary_text = l10n_util::GetStringFUTF16Int(
      IDS_ASH_VIDEO_CONFERENCE_RETURN_TO_APP_SUMMARY_TEXT,
      static_cast<int>(apps.size()));

  // Note that the `app_type` parameter for the summary row is unused.
  summary_row_view_ =
      container_view_->AddChildView(std::make_unique<ReturnToAppButton>(
          /*panel=*/this,
          /*is_top_row=*/true, /*app_id=*/base::UnguessableToken::Null(),
          any_apps_capturing_camera, any_apps_capturing_microphone,
          any_apps_capturing_screen, summary_text,
          /*app_type=*/crosapi::mojom::VideoConferenceAppType::kDefaultValue));
  summary_row_view_->AddObserver(this);

  for (auto& app : apps) {
    container_view_->AddChildView(std::make_unique<ReturnToAppButton>(
        /*panel=*/this,
        /*is_top_row=*/false, app->id, app->is_capturing_camera,
        app->is_capturing_microphone, app->is_capturing_screen,
        GetMediaAppDisplayText(app), app->app_type));
  }
}

}  // namespace ash::video_conference
