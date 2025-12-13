// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_connection_dialog.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kCancelButtonId);

// static
DevToolsConnectionDialog* DevToolsConnectionDialog::Show(
    Browser* browser,
    DevToolsConnectionDialog::AcceptCallback callback) {
  return new DevToolsConnectionDialog(browser, std::move(callback));
}

DevToolsConnectionDialog::DevToolsConnectionDialog(
    Browser* browser,
    DevToolsConnectionDialog::AcceptCallback callback)
    : browser_(browser), callback_(std::move(callback)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!browser) {
    RunCallbackAndDie(
        content::DevToolsManagerDelegate::AcceptConnectionResult::kDeny);
    return;
  }

  if (browser->window()) {
    browser->window()->Activate();
  }

  views::Widget* widget = chrome::ShowBrowserModal(
      browser,
      ui::DialogModel::Builder()
          .SetTitle(
              l10n_util::GetStringUTF16(IDS_DEV_TOOLS_CONNECTION_DIALOG_TITLE))
          .AddParagraph(
              ui::DialogModelLabel(IDS_DEV_TOOLS_CONNECTION_DIALOG_MESSAGE_PART_1))
          .AddParagraph(
            ui::DialogModelLabel(IDS_DEV_TOOLS_CONNECTION_DIALOG_MESSAGE_PART_2))
          .AddOkButton(base::BindOnce(&DevToolsConnectionDialog::OnAccept,
                                      base::Unretained(this)),
                       ui::DialogModel::Button::Params()
                           .SetStyle(ui::ButtonStyle::kTonal)
                           .SetLabel(l10n_util::GetStringUTF16(
                               IDS_DEV_TOOLS_CONNECTION_DIALOG_ALLOW_TEXT)))
          .AddCancelButton(base::BindOnce(&DevToolsConnectionDialog::OnCancel,
                                          base::Unretained(this)),
                           ui::DialogModel::Button::Params()
                               .SetStyle(ui::ButtonStyle::kTonal)
                               .SetId(kCancelButtonId))
          .AddExtraButton(
              base::BindRepeating(&DevToolsConnectionDialog::OnDisable,
                                  base::Unretained(this)),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_DEV_TOOLS_CONNECTION_DIALOG_DISABLE_TEXT)))
          .SetCloseActionCallback(base::BindOnce(
              &DevToolsConnectionDialog::OnClose, base::Unretained(this)))
          .SetInitiallyFocusedField(kCancelButtonId)
          .OverrideDefaultButton(ui::mojom::DialogButton::kNone)
          .Build());
  dialog_widget_ = widget->GetWeakPtr();
}

DevToolsConnectionDialog::~DevToolsConnectionDialog() = default;

void DevToolsConnectionDialog::OnAccept() {
  RunCallbackAndDie(
      content::DevToolsManagerDelegate::AcceptConnectionResult::kAllow);
}

void DevToolsConnectionDialog::OnCancel() {
  RunCallbackAndDie(
      content::DevToolsManagerDelegate::AcceptConnectionResult::kDeny);
}

void DevToolsConnectionDialog::OnDisable(const ui::Event& event) {
  GURL internal_url("chrome://inspect#remote-debugging");
  NavigateParams params(browser_, internal_url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
  RunCallbackAndDie(
      content::DevToolsManagerDelegate::AcceptConnectionResult::kDeny);
}

void DevToolsConnectionDialog::OnClose() {
  RunCallbackAndDie(
      content::DevToolsManagerDelegate::AcceptConnectionResult::kDeny);
}

void DevToolsConnectionDialog::RunCallbackAndDie(
    content::DevToolsManagerDelegate::AcceptConnectionResult result) {
  if (handled_) {
    return;
  }
  handled_ = true;
  if (callback_) {
    std::move(callback_).Run(result);
  }
  if (dialog_widget_ && !dialog_widget_->IsClosed()) {
    dialog_widget_->Close();
  }
  delete this;
}
