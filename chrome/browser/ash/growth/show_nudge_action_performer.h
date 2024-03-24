// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GROWTH_SHOW_NUDGE_ACTION_PERFORMER_H_
#define CHROME_BROWSER_ASH_GROWTH_SHOW_NUDGE_ACTION_PERFORMER_H_

#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/growth/ui_action_performer.h"

// Dictionary of supported nudge payload. For example:
// {
//   "title": "Nudge title",
//   "body": "Body text"
// }
using NudgePayload = base::Value::Dict;

// Implements the action to show nudge.
class ShowNudgeActionPerformer : public UiActionPerformer {
 public:
  ShowNudgeActionPerformer();
  ~ShowNudgeActionPerformer() override;

  // growth::Action:
  void Run(int campaign_id,
           const base::Value::Dict* action_params,
           growth::ActionPerformer::Callback callback) override;
  growth::ActionType ActionType() const override;

 private:
  bool ShowNudge(int campaign_id, const NudgePayload* nudge_payload);
  void MaybeSetButtonData(int campaign_id,
                          const base::Value::Dict* button_dict,
                          ash::AnchoredNudgeData& nudge_data,
                          bool is_primary);
  void OnNudgeButtonClicked(int campaign_id,
                            const base::Value::Dict* action_dict);

  base::WeakPtrFactory<ShowNudgeActionPerformer> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_GROWTH_SHOW_NUDGE_ACTION_PERFORMER_H_
