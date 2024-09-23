// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GROWTH_OPEN_URL_ACTION_PERFORMER_H_
#define CHROME_BROWSER_ASH_GROWTH_OPEN_URL_ACTION_PERFORMER_H_

#include "base/functional/callback.h"
#include "base/values.h"
#include "chromeos/ash/components/growth/action_performer.h"

// Implements open URL action for the growth framework.
class OpenUrlActionPerformer : public growth::ActionPerformer {
 public:
  OpenUrlActionPerformer();
  ~OpenUrlActionPerformer() override;

  // growth::ActionPerformer:
  void Run(int campaign_id,
           std::optional<int> group_id,
           const base::Value::Dict* action_params,
           growth::ActionPerformer::Callback callback) override;
  growth::ActionType ActionType() const override;
};

#endif  // CHROME_BROWSER_ASH_GROWTH_OPEN_URL_ACTION_PERFORMER_H_
