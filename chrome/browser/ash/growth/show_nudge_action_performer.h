/// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GROWTH_SHOW_NUDGE_ACTION_PERFORMER_H_
#define CHROME_BROWSER_ASH_GROWTH_SHOW_NUDGE_ACTION_PERFORMER_H_

#include "base/functional/callback.h"
#include "base/values.h"
#include "chromeos/ash/components/growth/action_performer.h"

// Dictionary of supported nudge payload. For example:
// {
//   "title": "Nudge title",
//   "body": "Body text"
// }
using NudgePayload = base::Value::Dict;

// Implements the action to show nudge.
class ShowNudgeActionPerformer : public growth::ActionPerformer {
 public:
  ShowNudgeActionPerformer();
  ~ShowNudgeActionPerformer() override;

  // growth::Action:
  void Run(int campaign_id,
           const base::Value::Dict* action_params,
           growth::ActionPerformer::Callback callback) override;
  growth::ActionType ActionType() const override;

 private:
  bool ShowNudge(const NudgePayload* nudge_payload);
};

#endif  // CHROME_BROWSER_ASH_GROWTH_SHOW_NUDGE_ACTION_PERFORMER_H_
