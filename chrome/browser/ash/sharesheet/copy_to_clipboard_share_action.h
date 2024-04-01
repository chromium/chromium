// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SHARESHEET_COPY_TO_CLIPBOARD_SHARE_ACTION_H_
#define CHROME_BROWSER_ASH_SHARESHEET_COPY_TO_CLIPBOARD_SHARE_ACTION_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/sharesheet/share_action/share_action.h"
#include "components/services/app_service/public/cpp/intent.h"

class Profile;

namespace ash {

struct ToastData;

namespace sharesheet {

class CopyToClipboardShareAction : public ::sharesheet::ShareAction {
 public:
  explicit CopyToClipboardShareAction(Profile* profile);
  ~CopyToClipboardShareAction() override;
  CopyToClipboardShareAction(const CopyToClipboardShareAction&) = delete;
  CopyToClipboardShareAction& operator=(const CopyToClipboardShareAction&) =
      delete;

  // ShareAction:
  ::sharesheet::ShareActionType GetActionType() const override;
  const std::u16string GetActionName() override;
  const gfx::VectorIcon& GetActionIcon() override;
  void LaunchAction(::sharesheet::SharesheetController* controller,
                    views::View* root_view,
                    apps::IntentPtr intent) override;
  void OnClosing(::sharesheet::SharesheetController* controller) override;

 private:
  // Virtual so that it can be overridden in testing.
  virtual void ShowToast(ash::ToastData toast_data);

  raw_ptr<Profile, DanglingUntriaged> profile_;
};

}  // namespace sharesheet
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SHARESHEET_COPY_TO_CLIPBOARD_SHARE_ACTION_H_
