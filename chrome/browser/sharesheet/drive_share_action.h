// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_DRIVE_SHARE_ACTION_H_
#define CHROME_BROWSER_SHARESHEET_DRIVE_SHARE_ACTION_H_

#include "chrome/browser/sharesheet/share_action.h"

class Profile;

namespace sharesheet {

class DriveShareAction : public ShareAction {
 public:
  explicit DriveShareAction(Profile* profile);
  ~DriveShareAction() override;
  DriveShareAction(const DriveShareAction&) = delete;
  DriveShareAction& operator=(const DriveShareAction&) = delete;

  // ShareAction:
  const std::u16string GetActionName() override;
  const gfx::VectorIcon& GetActionIcon() override;
  void LaunchAction(SharesheetController* controller,
                    views::View* root_view,
                    apps::mojom::IntentPtr intent) override;
  void OnClosing(SharesheetController* controller) override;
  bool ShouldShowAction(const apps::mojom::IntentPtr& intent,
                        bool contains_hosted_document) override;

 private:
  Profile* profile_;
  SharesheetController* controller_ = nullptr;
};

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_DRIVE_SHARE_ACTION_H_
