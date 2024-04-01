// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARE_ACTION_EXAMPLE_ACTION_H_
#define CHROME_BROWSER_SHARESHEET_SHARE_ACTION_EXAMPLE_ACTION_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/sharesheet/share_action/share_action.h"
#include "components/services/app_service/public/cpp/intent.h"

namespace sharesheet {

class ExampleAction : public ShareAction {
 public:
  ExampleAction();
  ~ExampleAction() override;
  ExampleAction(const ExampleAction&) = delete;
  ExampleAction& operator=(const ExampleAction&) = delete;

  // ShareAction:
  ShareActionType GetActionType() const override;
  const std::u16string GetActionName() override;
  const gfx::VectorIcon& GetActionIcon() override;
  void LaunchAction(SharesheetController* controller,
                    views::View* root_view,
                    apps::IntentPtr intent) override;
  void OnClosing(SharesheetController* controller) override;
  bool HasActionView() override;

 private:
  raw_ptr<SharesheetController> controller_ = nullptr;
  std::string name_;
};

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARE_ACTION_EXAMPLE_ACTION_H_
