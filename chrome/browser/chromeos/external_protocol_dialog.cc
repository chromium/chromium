// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/external_protocol_dialog.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/arc/intent_helper/arc_external_protocol_dialog.h"
#include "chrome/browser/chromeos/guest_os/guest_os_external_protocol_handler.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/sharing/click_to_call/feature.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/external_protocol_dialog.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

using content::WebContents;

namespace {

const int kMessageWidth = 400;

void OnArcHandled(const GURL& url,
                  const base::Optional<url::Origin>& initiating_origin,
                  int render_process_host_id,
                  int routing_id,
                  bool handled) {
  if (handled)
    return;

  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, routing_id);

  // Display the standard ExternalProtocolDialog if Guest OS has a handler.
  if (web_contents && base::FeatureList::IsEnabled(
                          chromeos::features::kGuestOsExternalProtocol)) {
    base::Optional<guest_os::GuestOsRegistryService::Registration>
        registration = guest_os::GetHandler(
            Profile::FromBrowserContext(web_contents->GetBrowserContext()),
            url);
    if (registration) {
      new ExternalProtocolDialog(web_contents, url,
                                 base::UTF8ToUTF16(registration->Name()),
                                 initiating_origin);
      return;
    }
  }
  new ExternalProtocolNoHandlersDialog(web_contents, url);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolHandler

// static
void ExternalProtocolHandler::RunExternalProtocolDialog(
    const GURL& url,
    WebContents* web_contents,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    const base::Optional<url::Origin>& initiating_origin) {
  // First, check if ARC version of the dialog is available and run ARC version
  // when possible.
  // TODO(ellyjones): Refactor arc::RunArcExternalProtocolDialog() to take a
  // web_contents directly, which will mean sorting out how lifetimes work in
  // that code. Same for OnArcHandled() (crbug.com/1136237).
  int render_process_host_id =
      web_contents->GetRenderViewHost()->GetProcess()->GetID();
  int routing_id = web_contents->GetRenderViewHost()->GetRoutingID();
  arc::RunArcExternalProtocolDialog(
      url, initiating_origin, render_process_host_id, routing_id,
      page_transition, has_user_gesture,
      base::BindOnce(&OnArcHandled, url, initiating_origin,
                     render_process_host_id, routing_id));
}

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolNoHandlersDialog

ExternalProtocolNoHandlersDialog::ExternalProtocolNoHandlersDialog(
    WebContents* web_contents,
    const GURL& url)
    : creation_time_(base::TimeTicks::Now()), scheme_(url.scheme()) {
  SetOwnedByWidget(true);

  views::DialogDelegate::SetButtons(ui::DIALOG_BUTTON_OK);
  views::DialogDelegate::SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_EXTERNAL_PROTOCOL_CLOSE_BUTTON_TEXT));

  message_box_view_ = new views::MessageBoxView();
  message_box_view_->SetMessageWidth(kMessageWidth);

  gfx::NativeWindow parent_window;
  if (web_contents) {
    parent_window = web_contents->GetTopLevelNativeWindow();
  } else {
    // Dialog is top level if we don't have a web_contents associated with us.
    parent_window = nullptr;
  }
  views::DialogDelegate::CreateDialogWidget(this, nullptr, parent_window)
      ->Show();
  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::EXTERNAL_PROTOCOL_CHROMEOS);
}

ExternalProtocolNoHandlersDialog::~ExternalProtocolNoHandlersDialog() = default;

base::string16 ExternalProtocolNoHandlersDialog::GetWindowTitle() const {
  // If click to call feature is available, we display a message to the user on
  // how to use the feature.
  // TODO(crbug.com/1007995) - This is a hotfix for M78 and we plan to use our
  // own dialog with more information in the future.
  if (scheme_ == url::kTelScheme &&
      base::FeatureList::IsEnabled(kClickToCallUI)) {
    return l10n_util::GetStringUTF16(
        IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_HELP_TEXT_NO_DEVICES);
  }
  return l10n_util::GetStringUTF16(IDS_EXTERNAL_PROTOCOL_NO_HANDLER_TITLE);
}

views::View* ExternalProtocolNoHandlersDialog::GetContentsView() {
  return message_box_view_;
}

const views::Widget* ExternalProtocolNoHandlersDialog::GetWidget() const {
  return message_box_view_->GetWidget();
}

views::Widget* ExternalProtocolNoHandlersDialog::GetWidget() {
  return message_box_view_->GetWidget();
}
