// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/show_nudge_action_performer.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/shell.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "chromeos/ash/components/growth/campaigns_manager.h"
#include "chromeos/ash/components/growth/campaigns_model.h"
#include "ui/views/bubble/bubble_border.h"

namespace {

// Nudge payload paths.
constexpr char kNudgeTitlePath[] = "title";
constexpr char kNudgeBodyPath[] = "body";
constexpr char kImagePath[] = "image";
constexpr char kDurationPath[] = "duration";
constexpr char kPrimaryButtonPath[] = "primaryButton";
constexpr char kSecondaryButtonPath[] = "secondaryButton";
constexpr char kLabelPath[] = "label";
constexpr char kActionPath[] = "action";
constexpr char kArrowPath[] = "arrow";

// Nudge ID.
constexpr char kGrowthNudgeId[] = "growth_campaign_nudge";

// These values are deserialized from Growth Campaign, so entries should not
// be renumbered and numeric values should never be reused.
enum class NudgeDuration { kDefaultDuration, kMediumDuration, kLongDuration };

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
  kFloat
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

  auto image_model = growth::Image(image_value).GetImage();
  if (!image_model) {
    // No image model matched the image payload.
    // TODO(b/329666969): Record invalid image model error.
    return;
  }

  nudge_data.image_model = image_model.value();
}

}  // namespace

ShowNudgeActionPerformer::ShowNudgeActionPerformer() = default;

ShowNudgeActionPerformer::~ShowNudgeActionPerformer() = default;

void ShowNudgeActionPerformer::Run(
  int campaign_id,
  const base::Value::Dict* action_params,
  growth::ActionPerformer::Callback callback) {
  if (!ShowNudge(campaign_id, action_params)) {
    std::move(callback).Run(growth::ActionResult::kFailure,
                            growth::ActionResultReason::kParsingActionFailed);
    return;
  }
  std::move(callback).Run(growth::ActionResult::kSuccess,
                          /*action_result_reason=*/std::nullopt);
}

growth::ActionType ShowNudgeActionPerformer::ActionType() const {
  return growth::ActionType::kShowNudge;
}

bool ShowNudgeActionPerformer::ShowNudge(int campaign_id,
                                         const NudgePayload* nudge_payload) {
  if (!nudge_payload) {
    return false;
  }

  auto* body_text = GetNudgeBody(nudge_payload);
  if (!body_text) {
    // TODO(b/330378048): Records parsing error.
    return false;
  }

  std::u16string nudge_body = base::UTF8ToUTF16(*body_text);

  // TODO: b/329701489 - Getting nudge anchor view.
  auto nudge_data = ash::AnchoredNudgeData(
      kGrowthNudgeId, ash::NudgeCatalogName::kGrowthCampaignNudge, nudge_body,
      /*anchor_view=*/nullptr);

  auto* title = GetNudgeTitle(nudge_payload);
  if (title && !title->empty()) {
    nudge_data.title_text = base::UTF8ToUTF16(*title);
  }

  // Set duration.
  auto duration_value = nudge_payload->FindInt(kDurationPath)
                            .value_or(int(NudgeDuration::kDefaultDuration));
  nudge_data.duration =
      ConvertDuration(static_cast<NudgeDuration>(duration_value));

  // Add buttons if available.
  MaybeSetButtonData(campaign_id, nudge_payload->FindDict(kPrimaryButtonPath),
                     nudge_data,
                     /*is_primary=*/true);
  MaybeSetButtonData(campaign_id, nudge_payload->FindDict(kSecondaryButtonPath),
                     nudge_data,
                     /*is_primary=*/false);

  // Set image data if available.
  MaybeSetImageData(nudge_payload->FindDict(kImagePath), nudge_data);

  // Set arrow.
  auto arrow_value =
      nudge_payload->FindInt(kArrowPath).value_or(int(Arrow::kBottomRight));
  nudge_data.arrow = ConvertArrow(static_cast<Arrow>(arrow_value));

  ash::Shell::Get()->anchored_nudge_manager()->Show(nudge_data);

  return true;
}

void ShowNudgeActionPerformer::MaybeSetButtonData(
    int campaign_id,
    const base::Value::Dict* button_dict,
    ash::AnchoredNudgeData& nudge_data,
    bool is_primary) {
  const auto* button_text_value = button_dict->FindString(kLabelPath);
  const auto* action = button_dict->FindDict(kActionPath);
  if (!button_text_value || button_text_value->empty() || !action) {
    return;
  }

  auto button_text = base::UTF8ToUTF16(*button_text_value);
  auto callback =
      base::BindRepeating(&ShowNudgeActionPerformer::OnNudgeButtonClicked,
                          weak_ptr_factory_.GetWeakPtr(), campaign_id, action);
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
    const base::Value::Dict* action_dict) {
  if (!action_dict) {
    return;
  }

  auto action = growth::Action(action_dict);
  auto action_type = action.GetActionType();
  if (!action_type) {
    return;
  }

  if (action_type.value() == growth::ActionType::kDismiss) {
    ash::Shell::Get()->anchored_nudge_manager()->Cancel(kGrowthNudgeId);

    // TODO(b/329671682): Log metrics.
    return;
  }

  auto* campaigns_manager = growth::CampaignsManager::Get();
  CHECK(campaigns_manager);

  campaigns_manager->PerformAction(campaign_id, &action);
}
