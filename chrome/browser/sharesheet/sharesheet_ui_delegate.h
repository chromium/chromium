// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARESHEET_UI_DELEGATE_H_
#define CHROME_BROWSER_SHARESHEET_SHARESHEET_UI_DELEGATE_H_

#include <vector>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/sharesheet/sharesheet_controller.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "ui/gfx/native_widget_types.h"

namespace sharesheet {

// The SharesheetUiDelegate is the interface for the controller used by
// the ShareService to request changes to the UI. This class needs to be
// implemented by each share bubble.
class SharesheetUiDelegate : public SharesheetController {
 public:
  ~SharesheetUiDelegate() override = default;

  virtual void ShowBubble(std::vector<TargetInfo> targets,
                          apps::IntentPtr intent,
                          DeliveredCallback delivered_callback,
                          CloseCallback close_callback) = 0;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Skips the generic Sharesheet bubble and directly displays the
  // NearbyShare bubble dialog.
  virtual void ShowNearbyShareBubbleForArc(apps::IntentPtr intent,
                                           DeliveredCallback delivered_callback,
                                           CloseCallback close_callback) = 0;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Invoked immediately after an action has launched in the event that UI
  // changes need to occur at this point.
  virtual void OnActionLaunched(bool has_action_view) {}
};

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARESHEET_UI_DELEGATE_H_
