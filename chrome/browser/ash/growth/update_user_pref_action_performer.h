// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GROWTH_UPDATE_USER_PREF_ACTION_PERFORMER_H_
#define CHROME_BROWSER_ASH_GROWTH_UPDATE_USER_PREF_ACTION_PERFORMER_H_

#include "base/values.h"
#include "chrome/browser/ash/growth/ui_action_performer.h"

// Implements update user preference action for the growth framework.
class UpdateUserPrefActionPerformer : public growth::ActionPerformer {
 public:
  UpdateUserPrefActionPerformer();
  ~UpdateUserPrefActionPerformer() override;

  // growth::ActionPerformer:
  void Run(int campaign_id,
           std::optional<int> group_id,
           const base::Value::Dict* action_params,
           growth::ActionPerformer::Callback callback) override;
  growth::ActionType ActionType() const override;
};

#endif  // CHROME_BROWSER_ASH_GROWTH_UPDATE_USER_PREF_ACTION_PERFORMER_H_
