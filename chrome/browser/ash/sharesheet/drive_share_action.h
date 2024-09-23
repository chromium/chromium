// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SHARESHEET_DRIVE_SHARE_ACTION_H_
#define CHROME_BROWSER_ASH_SHARESHEET_DRIVE_SHARE_ACTION_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/sharesheet/share_action/share_action.h"
#include "components/services/app_service/public/cpp/intent.h"

namespace ash {
namespace sharesheet {

class DriveShareAction : public ::sharesheet::ShareAction {
 public:
  DriveShareAction();
  ~DriveShareAction() override;
  DriveShareAction(const DriveShareAction&) = delete;
  DriveShareAction& operator=(const DriveShareAction&) = delete;

  // ShareAction:
  ::sharesheet::ShareActionType GetActionType() const override;
  const std::u16string GetActionName() override;
  const gfx::VectorIcon& GetActionIcon() override;
  void LaunchAction(::sharesheet::SharesheetController* controller,
                    views::View* root_view,
                    apps::IntentPtr intent) override;
  void OnClosing(::sharesheet::SharesheetController* controller) override;
  bool ShouldShowAction(const apps::IntentPtr& intent,
                        bool contains_hosted_document) override;

 private:
  raw_ptr<::sharesheet::SharesheetController> controller_ = nullptr;
};

}  // namespace sharesheet
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SHARESHEET_DRIVE_SHARE_ACTION_H_
