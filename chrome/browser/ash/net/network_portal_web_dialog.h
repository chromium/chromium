// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_PORTAL_WEB_DIALOG_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_PORTAL_WEB_DIALOG_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/ash/network/network_portal_notification_controller.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace views {
class Widget;
}

namespace ash {

// This is the modal Web dialog to display captive portal login page.
// It is automatically closed when successful authorization is detected.
class NetworkPortalWebDialog : public ui::WebDialogDelegate {
 public:
  explicit NetworkPortalWebDialog(
      base::WeakPtr<NetworkPortalNotificationController> controller);

  NetworkPortalWebDialog(const NetworkPortalWebDialog&) = delete;
  NetworkPortalWebDialog& operator=(const NetworkPortalWebDialog&) = delete;

  ~NetworkPortalWebDialog() override;

  void SetWidget(views::Widget* widget);
  void Close();

 private:
  // ui::WebDialogDelegate:
  ui::ModalType GetDialogModalType() const override;
  std::u16string GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;

  base::WeakPtr<NetworkPortalNotificationController> controller_;

  views::Widget* widget_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace chromeos {
using ::ash::NetworkPortalWebDialog;
}

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_PORTAL_WEB_DIALOG_H_
