// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/show_nudge_action_performer.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/shell.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"

namespace {

// Nudge payload paths.
inline constexpr char kNudgeTitle[] = "title";
inline constexpr char kNudgeBody[] = "body";

const std::string* GetNudgeTitle(const NudgePayload* nudge_payload) {
  CHECK(nudge_payload);
  return nudge_payload->FindString(kNudgeTitle);
}

const std::string* GetNudgeBody(const NudgePayload* nudge_payload) {
  CHECK(nudge_payload);
  return nudge_payload->FindString(kNudgeBody);
}

}  // namespace

ShowNudgeActionPerformer::ShowNudgeActionPerformer() = default;

ShowNudgeActionPerformer::~ShowNudgeActionPerformer() = default;

void ShowNudgeActionPerformer::Run(
  int campaign_id,
  const base::Value::Dict* action_params,
  growth::ActionPerformer::Callback callback) {
  if (!ShowNudge(action_params)) {
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

bool ShowNudgeActionPerformer::ShowNudge(const NudgePayload* nudge_payload) {
  if (!nudge_payload) {
    return false;
  }

  auto* body_text = GetNudgeBody(nudge_payload);
  if (!body_text) {
    // TODO(b/330378048): Records parsing error.
    return false;
  }

  std::u16string nudge_body = base::UTF8ToUTF16(*body_text);

  // TODO: b/329893738 - Create unique id for different growth nudges.
  const std::string id = "campaign_nudge";

  // TODO: b/329701489 - Getting nudge anchor view.
  auto nudge_data = ash::AnchoredNudgeData(
      id, ash::NudgeCatalogName::kGrowthCampaignNudge, nudge_body,
      /*anchor_view=*/nullptr);

  auto* title = GetNudgeTitle(nudge_payload);
  if (title && !title->empty()) {
    nudge_data.title_text = base::UTF8ToUTF16(*title);
  }

  // TODO: b/329701349 - Update nudge duration.
  nudge_data.duration = ash::NudgeDuration::kMediumDuration;
  ash::Shell::Get()->anchored_nudge_manager()->Show(nudge_data);

  return true;
}
