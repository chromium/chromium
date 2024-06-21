// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GROWTH_INSTALL_WEB_APP_ACTION_PERFORMER_H_
#define CHROME_BROWSER_ASH_GROWTH_INSTALL_WEB_APP_ACTION_PERFORMER_H_

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/values.h"
#include "chromeos/ash/components/growth/action_performer.h"
#include "components/webapps/browser/install_result_code.h"

// Implements Web App installation action for the growth framework.
class InstallWebAppActionPerformer : public growth::ActionPerformer {
 public:
  InstallWebAppActionPerformer();
  InstallWebAppActionPerformer(const InstallWebAppActionPerformer&) = delete;
  InstallWebAppActionPerformer& operator=(const InstallWebAppActionPerformer&) =
      delete;
  ~InstallWebAppActionPerformer() override;

  // growth::ActionPerformer:
  void Run(int campaign_id,
           std::optional<int> group_id,
           const base::Value::Dict* action_params,
           growth::ActionPerformer::Callback callback) override;
  growth::ActionType ActionType() const override;
};

#endif  // CHROME_BROWSER_ASH_GROWTH_INSTALL_WEB_APP_ACTION_PERFORMER_H_
