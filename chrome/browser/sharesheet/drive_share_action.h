// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_DRIVE_SHARE_ACTION_H_
#define CHROME_BROWSER_SHARESHEET_DRIVE_SHARE_ACTION_H_

#include "chrome/browser/sharesheet/share_action.h"

class DriveShareAction : public sharesheet::ShareAction {
 public:
  DriveShareAction();
  ~DriveShareAction() override;
  DriveShareAction(const DriveShareAction&) = delete;
  DriveShareAction& operator=(const DriveShareAction&) = delete;

  // sharesheet::ShareAction:
  const base::string16 GetActionName() override;
  const gfx::ImageSkia GetActionIcon() override;
  void LaunchAction(sharesheet::SharesheetController* controller,
                    views::View* root_view,
                    apps::mojom::IntentPtr intent) override;
  void OnClosing(sharesheet::SharesheetController* controller) override;
  bool ShouldShowAction(const apps::mojom::IntentPtr& intent,
                        bool contains_hosted_document) override;

 private:
  sharesheet::SharesheetController* controller_ = nullptr;
};

#endif  // CHROME_BROWSER_SHARESHEET_DRIVE_SHARE_ACTION_H_
