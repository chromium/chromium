// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_DELEGATOR_H_
#define CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_DELEGATOR_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/browser/sharesheet/sharesheet_ui_delegate.h"
#include "chromeos/components/sharesheet/constants.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ui {
class Accelerator;
}  // namespace ui

namespace views {
class View;
}  // namespace views

namespace sharesheet {

class SharesheetService;

// The SharesheetServiceDelegator is the interface through which the business
// logic in SharesheetService communicates with the UI.
class SharesheetServiceDelegator {
 public:
  SharesheetServiceDelegator(gfx::NativeWindow native_window,
                             SharesheetService* sharesheet_service);
  ~SharesheetServiceDelegator();
  SharesheetServiceDelegator(const SharesheetServiceDelegator&) = delete;
  SharesheetServiceDelegator& operator=(const SharesheetServiceDelegator&) =
      delete;

  gfx::NativeWindow GetNativeWindow();
  SharesheetController* GetSharesheetController();

  // TODO(crbug.com/40191717) : Remove after business logic is moved
  // out of SharesheetHeaderView.
  Profile* GetProfile();

  SharesheetUiDelegate* GetUiDelegateForTesting();

  // ==========================================================================
  // ======================== SHARESHEET SERVICE TO UI ========================
  // ==========================================================================

  // The following are called by the ShareService to communicate with the UI.
  void ShowBubble(std::vector<TargetInfo> targets,
                  apps::IntentPtr intent,
                  DeliveredCallback delivered_callback,
                  CloseCallback close_callback);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Skips the generic Sharesheet bubble and directly displays the
  // NearbyShare bubble dialog.
  void ShowNearbyShareBubbleForArc(apps::IntentPtr intent,
                                   DeliveredCallback delivered_callback,
                                   CloseCallback close_callback);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Invoked immediately after an action has launched in the event that UI
  // changes need to occur at this point.
  void OnActionLaunched(bool has_action_view);

  void CloseBubble(SharesheetResult result);

  // ==========================================================================
  // ======================== UI TO SHARESHEET SERVICE ========================
  // ==========================================================================
  // The following are called by the UI to communicate with the ShareService.
  void OnBubbleClosed(const std::optional<ShareActionType>& share_action_type);
  void OnTargetSelected(const TargetType type,
                        const std::optional<ShareActionType>& share_action_type,
                        const std::optional<std::u16string>& app_name,
                        apps::IntentPtr intent,
                        views::View* share_action_view);
  bool OnAcceleratorPressed(const ui::Accelerator& accelerator,
                            const ShareActionType share_action_type);
  const gfx::VectorIcon* GetVectorIcon(
      const std::optional<ShareActionType>& share_action_type);

 private:
  // Only used for ID purposes. NativeWindow will always outlive the
  // SharesheetServiceDelegator.
  gfx::NativeWindow native_window_;

  raw_ptr<SharesheetService> sharesheet_service_;

  std::unique_ptr<SharesheetUiDelegate> sharesheet_controller_;
};

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_DELEGATOR_H_
