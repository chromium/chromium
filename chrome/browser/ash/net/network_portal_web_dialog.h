// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_PORTAL_WEB_DIALOG_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_PORTAL_WEB_DIALOG_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "url/gurl.h"

namespace views {
class Widget;
}

namespace ash {

// This is the modal Web dialog to display captive portal login page.
// It is automatically closed when successful authorization is detected.
class NetworkPortalWebDialog : public ui::WebDialogDelegate {
 public:
  class Delegate {
   public:
    Delegate() = default;
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    virtual ~Delegate() = default;

    // Called once when the dialog is destroyed.
    virtual void OnDialogDestroyed(const NetworkPortalWebDialog* dialog) = 0;
  };

  NetworkPortalWebDialog(const GURL& url, base::WeakPtr<Delegate> delegate);
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

  GURL url_;
  base::WeakPtr<Delegate> delegate_;
  raw_ptr<views::Widget, ExperimentalAsh> widget_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_PORTAL_WEB_DIALOG_H_
