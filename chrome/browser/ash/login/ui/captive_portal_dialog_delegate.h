// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_CAPTIVE_PORTAL_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_CAPTIVE_PORTAL_DIALOG_DELEGATE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class WebDialogView;
class Widget;
}  // namespace views

namespace ash {

// Handles captive portal UI. Hosts modal dialogs that might be opened by the
// web content.
class CaptivePortalDialogDelegate
    : public ui::WebDialogDelegate,
      public ChromeWebModalDialogManagerDelegate,
      public web_modal::WebContentsModalDialogHost {
 public:
  explicit CaptivePortalDialogDelegate(views::WebDialogView* host_dialog_view);
  CaptivePortalDialogDelegate(const CaptivePortalDialogDelegate&) = delete;
  CaptivePortalDialogDelegate& operator=(const CaptivePortalDialogDelegate&) =
      delete;
  ~CaptivePortalDialogDelegate() override;

  void Show();

  void Hide();

  void Close();

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

  // ChromeWebModalDialogManagerDelegate:
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

  // web_modal::WebContentsModalDialogHost:
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

  base::WeakPtr<CaptivePortalDialogDelegate> GetWeakPtr();

  views::Widget* widget_for_test() { return widget_; }
  content::WebContents* web_contents_for_test() { return web_contents_; }

 private:
  raw_ptr<views::Widget, ExperimentalAsh> widget_ = nullptr;
  raw_ptr<views::WebDialogView, ExperimentalAsh> view_ = nullptr;
  raw_ptr<views::WebDialogView, ExperimentalAsh> host_view_ = nullptr;
  raw_ptr<content::WebContents, ExperimentalAsh> web_contents_ = nullptr;

  class ModalDialogManagerCleanup;
  std::unique_ptr<ModalDialogManagerCleanup> modal_dialog_manager_cleanup_;
  base::ObserverList<web_modal::ModalDialogHostObserver>::Unchecked
      modal_dialog_host_observer_list_;

  base::WeakPtrFactory<CaptivePortalDialogDelegate> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_CAPTIVE_PORTAL_DIALOG_DELEGATE_H_
