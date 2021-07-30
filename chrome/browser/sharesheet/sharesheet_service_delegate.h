// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/sharesheet/sharesheet_controller.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class View;
}  // namespace views

namespace sharesheet {

class SharesheetService;

// The SharesheetServiceDelegate is the interface through which the business
// logic in SharesheetService communicates with the UI.
class SharesheetServiceDelegate : public ::sharesheet::SharesheetController {
 public:
  SharesheetServiceDelegate(gfx::NativeWindow native_window,
                            SharesheetService* sharesheet_service);
  ~SharesheetServiceDelegate() override = default;
  SharesheetServiceDelegate(const SharesheetServiceDelegate&) = delete;
  SharesheetServiceDelegate& operator=(const SharesheetServiceDelegate&) =
      delete;

  gfx::NativeWindow GetNativeWindow();

  // The following are called by the ShareService to communicate with the UI.
  virtual void ShowBubble(std::vector<TargetInfo> targets,
                          apps::mojom::IntentPtr intent,
                          sharesheet::DeliveredCallback delivered_callback,
                          sharesheet::CloseCallback close_callback);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  virtual void ShowNearbyShareBubble(
      apps::mojom::IntentPtr intent,
      sharesheet::DeliveredCallback delivered_callback,
      sharesheet::CloseCallback close_callback) = 0;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Invoked immediately after an action has launched in the event that UI
  // changes need to occur at this point.
  virtual void OnActionLaunched();

  // The following are called by the UI to communicate with the ShareService.
  void OnBubbleClosed(const std::u16string& active_action);
  void OnTargetSelected(const std::u16string& target_name,
                        const TargetType type,
                        apps::mojom::IntentPtr intent,
                        views::View* share_action_view);
  bool OnAcceleratorPressed(const ui::Accelerator& accelerator,
                            const std::u16string& active_action);
  const gfx::VectorIcon* GetVectorIcon(const std::u16string& display_name);

  // SharesheetController:
  Profile* GetProfile() override;
  // Default implementation does nothing. Override as needed.
  void SetSharesheetSize(int width, int height) override;
  // Default implementation does nothing. Override as needed.
  void CloseSharesheet(SharesheetResult result) override;

 private:
  // Only used for ID purposes. NativeWindow will always outlive the
  // SharesheetServiceDelegate.
  gfx::NativeWindow native_window_;

  // Owned by views.
  SharesheetService* sharesheet_service_;
};

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_DELEGATE_H_
