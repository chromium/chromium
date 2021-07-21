// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SHARESHEET_CROS_SHARESHEET_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_ASH_SHARESHEET_CROS_SHARESHEET_SERVICE_DELEGATE_H_

#include "chrome/browser/sharesheet/sharesheet_controller.h"
#include "chrome/browser/sharesheet/sharesheet_service_delegate.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/gfx/native_widget_types.h"

namespace ash {
namespace sharesheet {

class SharesheetBubbleView;

// The Chrome OS only SharesheetServiceDelegate class.
// CrosSharesheetServiceDelegate is the interface through which the business
// logic in the SharesheetService communicates with the UI
// (SharesheetBubbleView).
class CrosSharesheetServiceDelegate
    : public ::sharesheet::SharesheetServiceDelegate {
 public:
  CrosSharesheetServiceDelegate(
      gfx::NativeWindow native_window,
      ::sharesheet::SharesheetService* sharesheet_service);
  ~CrosSharesheetServiceDelegate() override = default;
  CrosSharesheetServiceDelegate(const CrosSharesheetServiceDelegate&) = delete;
  CrosSharesheetServiceDelegate& operator=(
      const CrosSharesheetServiceDelegate&) = delete;

  // ::sharesheet::SharesheetServiceDelegate overrides:
  void ShowBubble(std::vector<::sharesheet::TargetInfo> targets,
                  apps::mojom::IntentPtr intent,
                  ::sharesheet::DeliveredCallback delivered_callback,
                  ::sharesheet::CloseCallback close_callback) override;
  void ShowNearbyShareBubble(
      apps::mojom::IntentPtr intent,
      ::sharesheet::DeliveredCallback delivered_callback,
      ::sharesheet::CloseCallback close_callback) override;
  void OnActionLaunched() override;

  // ::sharesheet::SharesheetController overrides:
  void SetSharesheetSize(int width, int height) override;
  void CloseSharesheet(::sharesheet::SharesheetResult result) override;

 private:
  bool IsBubbleVisible() const;

  // Owned by views.
  SharesheetBubbleView* sharesheet_bubble_view_;
};

}  // namespace sharesheet
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SHARESHEET_CROS_SHARESHEET_SERVICE_DELEGATE_H_
