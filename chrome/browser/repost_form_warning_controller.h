// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REPOST_FORM_WARNING_CONTROLLER_H_
#define CHROME_BROWSER_REPOST_FORM_WARNING_CONTROLLER_H_

#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"

// This class is used to continue or cancel a pending reload when the
// repost form warning is shown. It is owned by the platform-specific
// |TabModalConfirmDialog{Gtk, Mac, Views, WebUI}| classes.
class RepostFormWarningController : public TabModalConfirmDialogDelegate {
 public:
  explicit RepostFormWarningController(content::WebContents* web_contents);

  RepostFormWarningController(const RepostFormWarningController&) = delete;
  RepostFormWarningController& operator=(const RepostFormWarningController&) =
      delete;

  ~RepostFormWarningController() override;

 private:
  // TabModalConfirmDialogDelegate methods:
  std::u16string GetTitle() override;
  std::u16string GetDialogMessage() override;
  std::u16string GetAcceptButtonTitle() override;
  void OnAccepted() override;
  void OnCanceled() override;
  void OnClosed() override;

  // content::WebContentsObserver methods via TabModalConfirmDialogDelegate:
  void BeforeFormRepostWarningShow() override;
};

#endif  // CHROME_BROWSER_REPOST_FORM_WARNING_CONTROLLER_H_
