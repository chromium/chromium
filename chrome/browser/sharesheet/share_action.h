// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARE_ACTION_H_
#define CHROME_BROWSER_SHARESHEET_SHARE_ACTION_H_

#include <string>

#include "chrome/browser/sharesheet/sharesheet_controller.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/views/view.h"

namespace gfx {
struct VectorIcon;
}

namespace sharesheet {

// An interface implemented by each ShareAction.
class ShareAction {
 public:
  virtual ~ShareAction() = default;

  virtual const std::u16string GetActionName() = 0;

  virtual const gfx::VectorIcon& GetActionIcon() = 0;

  // LaunchAction should synchronously create all UI needed and fill
  // the |root_view|. Methods on |controller| can be used to inform
  // the sharesheet about the lifecycle of the ShareAction.
  //
  // |root_view| is a container within the larger share_sheet which should act
  // as the parent view for ShareAction views. It is guaranteed that
  // |root_view| and |controller| will stay alive and visible until either
  // ShareAction::OnClosing is called, or the ShareAction calls
  // |controller|->ShareActionCompleted().
  //
  // |intent| contains the data (including the file URLs) for the share action
  // to parse and interpret if needed for its UI and functionality.
  virtual void LaunchAction(SharesheetController* controller,
                            views::View* root_view,
                            apps::mojom::IntentPtr intent) = 0;

  // OnClosing informs the ShareAction when the sharesheet with |controller| is
  // closed. This occurs when the user presses the back button out of the share
  // action view or closes the sharesheet. All processes in ShareAction should
  // shutdown when OnClosing is called, and not use |root_view| or |controller|
  // once the method completes as they will be destroyed.
  virtual void OnClosing(SharesheetController* controller) = 0;

  // Return true if the action should be shown on the sharesheet. By default,
  // the actions are only visible if the files don't contain a Google Drive
  // hosted document.
  virtual bool ShouldShowAction(const apps::mojom::IntentPtr& intent,
                                bool contains_hosted_document);

  // Invoked when the accelerator has been pressed.
  // ShareAction should return true if the accelerator has been processed and
  // false otherwise. If not processed, the Sharesheet will close.
  virtual bool OnAcceleratorPressed(const ui::Accelerator& accelerator);
};

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARE_ACTION_H_
