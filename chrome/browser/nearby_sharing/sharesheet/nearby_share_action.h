// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_SHARESHEET_NEARBY_SHARE_ACTION_H_
#define CHROME_BROWSER_NEARBY_SHARING_SHARESHEET_NEARBY_SHARE_ACTION_H_

#include "chrome/browser/sharesheet/share_action.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share_dialog_ui.h"

class NearbyShareAction : public sharesheet::ShareAction,
                          nearby_share::NearbyShareDialogUI::Observer {
 public:
  NearbyShareAction();
  ~NearbyShareAction() override;
  NearbyShareAction(const NearbyShareAction&) = delete;
  NearbyShareAction& operator=(const NearbyShareAction&) = delete;

  // sharesheet::ShareAction:
  const base::string16 GetActionName() override;
  const gfx::ImageSkia GetActionIcon() override;
  void LaunchAction(sharesheet::SharesheetController* controller,
                    views::View* root_view,
                    apps::mojom::IntentPtr intent) override;
  void OnClosing(sharesheet::SharesheetController* controller) override;

  // nearby_share::NearbyShareDialogUI::Observer:
  void OnClose() override;

 private:
  sharesheet::SharesheetController* controller_ = nullptr;
  nearby_share::NearbyShareDialogUI* nearby_ui_ = nullptr;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_SHARESHEET_NEARBY_SHARE_ACTION_H_
