// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/show_nudge_action_performer.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "base/check_is_test.h"
#include "base/logging.h"
#include "chrome/browser/ash/growth/campaigns_manager_session.h"
#include "chrome/browser/ash/growth/metrics.h"
#include "chromeos/ash/components/growth/campaigns_constants.h"
#include "chromeos/ash/components/growth/campaigns_logger.h"
#include "chromeos/ash/components/growth/campaigns_manager.h"
#include "chromeos/ash/components/growth/campaigns_model.h"
#include "chromeos/ash/components/growth/growth_metrics.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "ui/aura/window.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// Anchored view for test.
std::optional<views::View*> anchored_view_for_testing = std::nullopt;

// Nudge payload paths.
constexpr char kNudgeTitlePath[] = "title";
constexpr char kNudgeBodyPath[] = "body";
constexpr char kImagePath[] = "image";
constexpr char kDurationPath[] = "duration";
constexpr char kClearEventsPath[] = "clearEvents";
constexpr char kLogCrOSEventsPath[] = "shouldLogCrOSEvents";
constexpr char kPrimaryButtonPath[] = "primaryButton";
constexpr char kSecondaryButtonPath[] = "secondaryButton";
constexpr char kLabelPath[] = "label";
constexpr char kActionPath[] = "action";
constexpr char kMarkDismissedPath[] = "shouldMarkDismissed";
constexpr char kArrowPath[] = "arrow";
constexpr char kAnchorPath[] = "anchor";

// Nudge ID.
constexpr char kGrowthNudgeId[] = "growth_campaign_nudge";

constexpr base::TimeDelta kCancelDelay = base::Milliseconds(100);

// These values are deserialized from Growth Campaign, so entries should not
// be renumbered and numeric values should never be reused.
enum class NudgeDuration {
  kDefaultDuration,
  kMediumDuration,
  kLongDuration,
  kMaxValue = kLongDuration
};

ash::NudgeDuration ConvertDuration(NudgeDuration duration) {
  switch (duration) {
    case NudgeDuration::kDefaultDuration:
      return ash::NudgeDuration::kDefaultDuration;
    case NudgeDuration::kMediumDuration:
      return ash::NudgeDuration::kMediumDuration;
    case NudgeDuration::kLongDuration:
      return ash::NudgeDuration::kLongDuration;
  }
}

// These values are deserialized from Growth Campaign, so entries should not
// be renumbered and numeric values should never be reused.
enum class Arrow {
  kTopLeft,
  kTopRight,
  kBottomLeft,
  kBottomRight,
  kLeftTop,
  kRightTop,
  kLeftBottom,
  kRightBottom,
  kTopCenter,
  kBottomCenter,
  kLeftCenter,
  kRightCenter,
  kNone,
  kFloat,
  kMaxValue = kFloat
};

views::BubbleBorder::Arrow ConvertArrow(Arrow arrow) {
  switch (arrow) {
    case Arrow::kTopLeft:
      return views::BubbleBorder::Arrow::TOP_LEFT;
    case Arrow::kTopRight:
      return views::BubbleBorder::Arrow::TOP_RIGHT;
    case Arrow::kBottomLeft:
      return views::BubbleBorder::Arrow::BOTTOM_LEFT;
    case Arrow::kBottomRight:
      return views::BubbleBorder::Arrow::BOTTOM_RIGHT;
    case Arrow::kLeftTop:
      return views::BubbleBorder::Arrow::LEFT_TOP;
    case Arrow::kRightTop:
      return views::BubbleBorder::Arrow::RIGHT_TOP;
    case Arrow::kLeftBottom:
      return views::BubbleBorder::Arrow::LEFT_BOTTOM;
    case Arrow::kRightBottom:
      return views::BubbleBorder::Arrow::RIGHT_BOTTOM;
    case Arrow::kTopCenter:
      return views::BubbleBorder::Arrow::TOP_CENTER;
    case Arrow::kBottomCenter:
      return views::BubbleBorder::Arrow::BOTTOM_CENTER;
    case Arrow::kLeftCenter:
      return views::BubbleBorder::Arrow::LEFT_CENTER;
    case Arrow::kRightCenter:
      return views::BubbleBorder::Arrow::RIGHT_CENTER;
    case Arrow::kNone:
      return views::BubbleBorder::Arrow::NONE;
    case Arrow::kFloat:
      return views::BubbleBorder::Arrow::FLOAT;
  }
}

const std::string* GetNudgeTitle(const NudgePayload* nudge_payload) {
  CHECK(nudge_payload);
  return nudge_payload->FindString(kNudgeTitlePath);
}

const std::string* GetNudgeBody(const NudgePayload* nudge_payload) {
  CHECK(nudge_payload);
  return nudge_payload->FindString(kNudgeBodyPath);
}

void MaybeSetImageData(const base::Value::Dict* image_value,
                       ash::AnchoredNudgeData& nudge_data) {
  if (!image_value) {
    return;
  }

  auto image_model = growth::ImageModel(image_value).GetImageModel();
  if (!image_model) {
    // No image model matched the image payload.
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kNudgePayloadInvalidImage);

    return;
  }

  nudge_data.image_model = image_model.value();
}

// Return the top level window widget.
views::Widget* GetTriggeringWindowWidget() {
  auto* session = CampaignsManagerSession::Get();
  if (!session) {
    CHECK_IS_TEST();
    return nullptr;
  }

  auto* window = session->GetOpenedWindow();
  if (!window) {
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kNoOpendedWindowToAnchor);
    CAMPAIGNS_LOG(ERROR) << "Error: No app window";
    return nullptr;
  }

  auto* widget =
      views::Widget::GetWidgetForNativeWindow(window->GetToplevelWindow());
  if (!widget) {
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kNoOpendedWindowWidgetToAnchor);
    CAMPAIGNS_LOG(ERROR) << "Error: widget not found";
    return nullptr;
  }

  return widget;
}

views::Widget* GetAnchoringWindowWidget() {
  // Currently, for all of our use cases, triggering window is same as
  // anchoring window, so we use trigger window for now.
  // In the future, if the triggering window is different the anchoring window,
  // we will use the anchoring window.
  //
  // TODO: b/370055198 - Return the top level window widget for anchoring
  // window.
  return GetTriggeringWindowWidget();
}

views::View* GetWindowCaptionButtonContainer() {
  if (anchored_view_for_testing) {
    return anchored_view_for_testing.value();
  }

  // Currently, nudge can only be triggered by app opened, so it is safe to
  // assume that the triggering window is the window to anchor on. If we adding
  // other triggering UI element, we need to revisit this decision.
  auto* targeting_window_widget = GetAnchoringWindowWidget();
  if (!targeting_window_widget) {
    return nullptr;
  }

  auto* root_view = targeting_window_widget->GetRootView();
  if (!root_view) {
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kNoRootViewToGetAnchorView);
    CAMPAIGNS_LOG(ERROR) << "Error: root view not found";
    return nullptr;
  }

  return root_view->GetViewByID(
      chromeos::ViewID::VIEW_ID_CAPTION_BUTTON_CONTAINER);
}

views::Widget* GetAnchorWidgetForWindowBoundAnchor(
    const views::BubbleBorder::Arrow& arrow) {
  // Currently the anchor widget is the triggering window widget.
  auto* anchor_widget = GetAnchoringWindowWidget();
  if (!anchor_widget) {
    // No targeted anchor widget found. Skip showing nudge.
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kNudgeAnchorWidgetNotFound);
    CAMPAIGNS_LOG(ERROR)
        << "Targeted anchor widget is not found. Skip showing nudge.";
    return nullptr;
  }

  // The arrow type will determine the nudge position in the widget, although
  // we do not draw the arrow. Only the two bottom corners are supported.
  switch (arrow) {
    case views::BubbleBorder::Arrow::BOTTOM_LEFT:
    case views::BubbleBorder::Arrow::LEFT_BOTTOM:
    case views::BubbleBorder::Arrow::BOTTOM_RIGHT:
    case views::BubbleBorder::Arrow::RIGHT_BOTTOM:
      break;
    default:
      // Other arrows are not supported. Skip showing nudge.
      growth::RecordCampaignsManagerError(
          growth::CampaignsManagerError::kNudgeAnchorPositionNotSupported);
      CAMPAIGNS_LOG(ERROR) << "Position is not supported. Skip showing nudge.";
      return nullptr;
  }
  return anchor_widget;
}

views::View* GetShelfAppButton(const std::string& app_id) {
  if (anchored_view_for_testing) {
    return anchored_view_for_testing.value();
  }

  return ash::Shell::GetPrimaryRootWindowController()
      ->shelf()
      ->hotseat_widget()
      ->GetShelfView()
      ->GetShelfAppButton(ash::ShelfID(app_id));
}

bool IsAnchorOnCaptionButtonContainer(
    std::optional<growth::WindowAnchorType> app_window_anchor_type) {
  return app_window_anchor_type &&
         app_window_anchor_type.value() ==
             growth::WindowAnchorType::kCaptionButtonContainer;
}

bool IsAnchorOnWindowBounds(
    std::optional<growth::WindowAnchorType> app_window_anchor_type) {
  return app_window_anchor_type && app_window_anchor_type.value() ==
                                       growth::WindowAnchorType::kWindowBounds;
}

}  // namespace

ShowNudgeActionPerformer::ShowNudgeActionPerformer() = default;

ShowNudgeActionPerformer::~ShowNudgeActionPerformer() {
  triggering_widget_ = nullptr;
}

void ShowNudgeActionPerformer::Run(int campaign_id,
                                   std::optional<int> group_id,
                                   const base::Value::Dict* action_params,
                                   growth::ActionPerformer::Callback callback) {
  if (!ShowNudge(campaign_id, group_id, action_params)) {
    // TODO: b/331953307 - callback with concrete failure result reason.
    std::move(callback).Run(growth::ActionResult::kFailure,
                            growth::ActionResultReason::kParsingActionFailed);
    return;
  }
  std::move(callback).Run(growth::ActionResult::kSuccess,
                          /*action_result_reason=*/std::nullopt);
}

void ShowNudgeActionPerformer::SetAnchoredViewForTesting(
    std::optional<views::View*> anchored_view) {
  anchored_view_for_testing = anchored_view;
}

growth::ActionType ShowNudgeActionPerformer::ActionType() const {
  return growth::ActionType::kShowNudge;
}

bool ShowNudgeActionPerformer::ShowNudge(int campaign_id,
                                         std::optional<int> group_id,
                                         const NudgePayload* nudge_payload) {
  if (!nudge_payload) {
    return false;
  }

  auto* body_text = GetNudgeBody(nudge_payload);
  if (!body_text) {
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kNudgePayloadMissingBody);
    return false;
  }

  std::u16string nudge_body = base::UTF8ToUTF16(*body_text);

  auto nudge_data = ash::AnchoredNudgeData(
      kGrowthNudgeId, ash::NudgeCatalogName::kGrowthCampaignNudge, nudge_body,
      /*anchor_view=*/nullptr);

  // Set arrow.
  auto arrow_value =
      nudge_payload->FindInt(kArrowPath).value_or(int(Arrow::kBottomRight));
  if (arrow_value >= 0 && arrow_value <= static_cast<int>(Arrow::kMaxValue)) {
    nudge_data.arrow = ConvertArrow(static_cast<Arrow>(arrow_value));
  }

  auto is_set_anchor_success =
      MaybeSetAnchorView(nudge_payload->FindDict(kAnchorPath), nudge_data);
  if (!is_set_anchor_success) {
    return false;
  }

  auto* title = GetNudgeTitle(nudge_payload);
  if (title && !title->empty()) {
    nudge_data.title_text = base::UTF8ToUTF16(*title);
  }

  // Set duration.
  auto duration_value = nudge_payload->FindInt(kDurationPath)
                            .value_or(int(NudgeDuration::kDefaultDuration));

  if (duration_value >= 0 &&
      duration_value <= static_cast<int>(NudgeDuration::kMaxValue)) {
    nudge_data.duration =
        ConvertDuration(static_cast<NudgeDuration>(duration_value));
  }

  // Default value of `should_log_cros_events` is false if this is not
  // configurated.
  const auto log_cros_events = nudge_payload->FindBool(kLogCrOSEventsPath);
  bool should_log_cros_events = log_cros_events.value_or(false);

  // Add buttons if available.
  MaybeSetButtonData(campaign_id, group_id,
                     nudge_payload->FindDict(kPrimaryButtonPath), nudge_data,
                     /*is_primary=*/true, should_log_cros_events);
  MaybeSetButtonData(campaign_id, group_id,
                     nudge_payload->FindDict(kSecondaryButtonPath), nudge_data,
                     /*is_primary=*/false, should_log_cros_events);

  // Set image data if available.
  MaybeSetImageData(nudge_payload->FindDict(kImagePath), nudge_data);

  // Set nudge dismiss callback.
  nudge_data.dismiss_callback =
      base::BindRepeating(&ShowNudgeActionPerformer::OnNudgeDismissed,
                          weak_ptr_factory_.GetWeakPtr(), campaign_id, group_id,
                          should_log_cros_events);

  // Shell may not be initialized in test.
  if (ash::Shell::HasInstance()) {
    ash::Shell::Get()->anchored_nudge_manager()->Show(nudge_data);
    if (ash::features::
            IsGrowthCampaignsObserveTriggeringWidgetChangeEnabled()) {
      MaybeSetWidgetObservers();
    }
  }

  // TODO: b/331045558 - Add close button callback.
  NotifyReadyToLogImpression(campaign_id, group_id, should_log_cros_events);

  const base::Value::List* clear_events =
      nudge_payload->FindList(kClearEventsPath);
  if (clear_events) {
    auto* campaigns_manager = growth::CampaignsManager::Get();
    CHECK(campaigns_manager);

    for (const auto& clear_event : *clear_events) {
      if (clear_event.is_string()) {
        campaigns_manager->ClearEvent(clear_event.GetString());
      }
    }
  }

  return true;
}

bool ShowNudgeActionPerformer::MaybeSetAnchorView(
    const base::Value::Dict* anchor_dict,
    ash::AnchoredNudgeData& nudge_data) {
  if (!anchor_dict) {
    // No anchor option provided. Set to default position.
    return true;
  }

  auto anchor = growth::Anchor(anchor_dict);
  auto window_anchor_type = anchor.GetActiveAppWindowAnchorType();
  if (window_anchor_type) {
    if (IsAnchorOnWindowBounds(window_anchor_type)) {
      if (!ash::features::
              IsGrowthCampaignsShowNudgeInsideWindowBoundsEnabled()) {
        return false;
      }

      auto* anchor_widget =
          GetAnchorWidgetForWindowBoundAnchor(nudge_data.arrow);
      if (!anchor_widget) {
        return false;
      }
      nudge_data.anchor_widget = anchor_widget;
    } else if (IsAnchorOnCaptionButtonContainer(window_anchor_type)) {
      auto* anchor_view = GetWindowCaptionButtonContainer();
      if (!anchor_view) {
        // No targeted anchor view found. Skip showing nudge.
        growth::RecordCampaignsManagerError(
            growth::CampaignsManagerError::kNudgeAnchorViewNotFound);
        CAMPAIGNS_LOG(ERROR)
            << "Targeted anchor view is not found. Skip showing nudge.";
        return false;
      }
      nudge_data.SetAnchorView(anchor_view);
    }

    nudge_data.set_anchor_view_as_parent = true;
  } else {
    auto* shelf_app_button_id = anchor.GetShelfAppButtonId();
    if (shelf_app_button_id) {
      auto* anchor_view = GetShelfAppButton(*shelf_app_button_id);
      if (!anchor_view) {
        // No targeted anchor view found. Skip showing nudge.
        growth::RecordCampaignsManagerError(
            growth::CampaignsManagerError::kNudgeSheflIconAnchorViewNotFound);
        CAMPAIGNS_LOG(ERROR) << "Targeted shelf icon anchor view is not found. "
                                "Skip showing nudge.";
        return false;
      }
      nudge_data.SetAnchorView(anchor_view);
      nudge_data.anchored_to_shelf = true;
    }
  }
  return true;
}

void ShowNudgeActionPerformer::MaybeSetButtonData(
    int campaign_id,
    std::optional<int> group_id,
    const base::Value::Dict* button_dict,
    ash::AnchoredNudgeData& nudge_data,
    bool is_primary,
    bool should_log_cros_events) {
  if (!button_dict) {
    return;
  }

  const auto* button_text_value = button_dict->FindString(kLabelPath);
  const auto* action = button_dict->FindDict(kActionPath);
  if (!button_text_value || button_text_value->empty() || !action) {
    return;
  }

  // Default value of `should_mark_dismissed` is false if this is not
  // configurated.
  const auto mark_dismissed = button_dict->FindBool(kMarkDismissedPath);
  bool should_mark_dismissed = mark_dismissed.value_or(false);

  auto button_text = base::UTF8ToUTF16(*button_text_value);
  auto callback = base::BindRepeating(
      &ShowNudgeActionPerformer::OnNudgeButtonClicked,
      weak_ptr_factory_.GetWeakPtr(), campaign_id, group_id,
      is_primary ? CampaignButtonId::kPrimary : CampaignButtonId::kSecondary,
      action, should_mark_dismissed, should_log_cros_events);
  if (is_primary) {
    nudge_data.primary_button_text = button_text;
    nudge_data.primary_button_callback = callback;
  } else {
    nudge_data.secondary_button_text = button_text;
    nudge_data.secondary_button_callback = callback;
  }
}

void ShowNudgeActionPerformer::OnNudgeButtonClicked(
    int campaign_id,
    std::optional<int> group_id,
    CampaignButtonId button_id,
    const base::Value::Dict* action_dict,
    bool should_mark_dismissed,
    bool should_log_cros_events) {
  NotifyButtonPressed(campaign_id, group_id, button_id, should_mark_dismissed,
                      should_log_cros_events);

  if (!action_dict) {
    return;
  }

  auto action = growth::Action(action_dict);
  auto action_type = action.GetActionType();
  if (!action_type) {
    return;
  }

  if (action_type.value() == growth::ActionType::kDismiss) {
    CancelNudge();

    // TODO(b/329671682): Log metrics.
    return;
  }

  auto* campaigns_manager = growth::CampaignsManager::Get();
  CHECK(campaigns_manager);

  campaigns_manager->PerformAction(campaign_id, group_id, &action);
}

void ShowNudgeActionPerformer::OnNudgeDismissed(int campaign_id,
                                                std::optional<int> group_id,
                                                bool should_log_cros_events) {
  // Dismissed automatically or by clicking on the X button. In this case, we
  // don't mark the nudge as dismissed and will resurface if impression
  // conditions met.
  NotifyDismissed(campaign_id, group_id, /*should_mark_dismissed=*/false,
                  should_log_cros_events);
}

void ShowNudgeActionPerformer::OnWidgetVisibilityChanged(views::Widget* widget,
                                                         bool visible) {
  if (!visible) {
    CancelNudge();
  }
}

void ShowNudgeActionPerformer::OnWidgetDestroying(views::Widget* widget) {
  CancelNudge();
}

void ShowNudgeActionPerformer::OnWidgetActivationChanged(views::Widget* widget,
                                                         bool active) {
  const auto* nudge =
      ash::Shell::Get()->anchored_nudge_manager()->GetNudgeIfShown(
          kGrowthNudgeId);
  if (nudge && widget == nudge->GetWidget()) {
    // Mark nudge activation state.
    is_nudge_active_ = active;
    return;
  }

  if (!active) {
    // Targeted app window widget is inactive, cancel the nudge in a delayed
    // task to make sure the `is_nudge_active` state is set before using.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ShowNudgeActionPerformer::MaybeCancelNudge,
                       weak_ptr_factory_.GetWeakPtr()),
        kCancelDelay);
  }
}

void ShowNudgeActionPerformer::MaybeSetWidgetObservers() {
  triggering_widget_ = GetTriggeringWindowWidget();
  if (triggering_widget_ == GetAnchoringWindowWidget()) {
    // The triggering widget is the same as anchoring widget, which is parent to
    // the nudge so the nudge won't show on top of other window if the
    // triggering widget becomes inactive or invisible.
    return;
  }

  // If the triggering and anchoring widget are not the same, we'll observe when
  // the triggering widget becomes inactive or invisible and close the nudge
  // accordingly to prevent it from showing on top of other window.
  //
  // TODO: b/370055198 - handle case of nudge closed by another nudge delayed
  // close event.
  if (triggering_widget_) {
    scoped_observation_.Observe(triggering_widget_);
  }

  auto* nudge = ash::Shell::Get()->anchored_nudge_manager()->GetNudgeIfShown(
      kGrowthNudgeId);
  if (nudge) {
    auto* nudge_widget = nudge->GetWidget();
    if (nudge_widget) {
      nudge_widget_scoped_observation_.Observe(nudge_widget);
    }
  }
}

void ShowNudgeActionPerformer::MaybeCancelNudge() {
  if (is_nudge_active_) {
    // The active widget is nudge. Skip canceling nudge.
    return;
  }

  CancelNudge();
}

void ShowNudgeActionPerformer::CancelNudge() {
  if (triggering_widget_) {
    scoped_observation_.Reset();
    triggering_widget_ = nullptr;
  }
  is_nudge_active_ = false;
  nudge_widget_scoped_observation_.Reset();

  ash::Shell::Get()->anchored_nudge_manager()->Cancel(kGrowthNudgeId);
}
