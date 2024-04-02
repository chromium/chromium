// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GROWTH_SHOW_NOTIFICATION_ACTION_PERFORMER_H_
#define CHROME_BROWSER_ASH_GROWTH_SHOW_NOTIFICATION_ACTION_PERFORMER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/growth/ui_action_performer.h"
#include "chromeos/ash/components/growth/campaigns_model.h"

// Implements show system notification action for the growth framework.
class ShowNotificationActionPerformer : public UiActionPerformer {
 public:
  ShowNotificationActionPerformer();
  ~ShowNotificationActionPerformer() override;

  // growth::Action:
  void Run(int campaign_id,
           const base::Value::Dict* action_params,
           growth::ActionPerformer::Callback callback) override;
  growth::ActionType ActionType() const override;

 private:
  void HandleNotificationClicked(const base::Value::Dict* params,
                                 const std::string& notification_id,
                                 int campaign_id,
                                 std::optional<int> button_index);

  base::WeakPtrFactory<ShowNotificationActionPerformer> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_GROWTH_SHOW_NOTIFICATION_ACTION_PERFORMER_H_
