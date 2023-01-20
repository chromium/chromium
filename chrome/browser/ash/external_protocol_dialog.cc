// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/external_protocol_dialog.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/arc/intent_helper/arc_intent_helper_mojo_ash.h"
#include "chrome/browser/ash/guest_os/guest_os_external_protocol_handler.h"
#include "chrome/browser/chromeos/arc/arc_external_protocol_dialog.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/external_protocol_dialog.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
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
                  const absl::optional<url::Origin>& initiating_origin,
                  content::WeakDocumentPtr initiator_document,
                  base::WeakPtr<WebContents> web_contents,
                  bool handled) {
  if (handled)
    return;

  // If WebContents have been destroyed, do not show any dialog.
  if (!web_contents)
    return;

  aura::Window* parent_window = web_contents->GetTopLevelNativeWindow();
  // If WebContents has been detached from window tree, do not show any dialog.
  if (!parent_window || !parent_window->GetRootWindow())
    return;

  // Display the standard ExternalProtocolDialog if Guest OS has a handler.
  // Otherwise, if there is no handler and the URL is a Tel-link, show the No
  // Handler Tel Scheme dialog
  absl::optional<guest_os::GuestOsRegistryService::Registration> registration =
      guest_os::GetHandler(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()), url);
  if (registration) {
    new ExternalProtocolDialog(web_contents.get(), url,
                               base::UTF8ToUTF16(registration->Name()),
                               initiating_origin, initiator_document);
  } else if (url.scheme() == url::kTelScheme) {
    new ash::ExternalProtocolNoHandlersTelSchemeDialog(parent_window);
  }
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
    bool is_in_fenced_frame_tree,
    const absl::optional<url::Origin>& initiating_origin,
    content::WeakDocumentPtr initiator_document,
    const std::u16string& program_name) {
  // First, check if ARC version of the dialog is available and run ARC version
  // when possible.
  arc::RunArcExternalProtocolDialog(
      url, initiating_origin, web_contents->GetWeakPtr(), page_transition,
      has_user_gesture, is_in_fenced_frame_tree,
      std::make_unique<arc::ArcIntentHelperMojoAsh>(),
      base::BindOnce(&OnArcHandled, url, initiating_origin,
                     std::move(initiator_document),
                     web_contents->GetWeakPtr()));
}

namespace ash {

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolNoHandlersTelSchemeDialog

ExternalProtocolNoHandlersTelSchemeDialog::
    ExternalProtocolNoHandlersTelSchemeDialog(aura::Window* parent_window)
    : creation_time_(base::TimeTicks::Now()) {
  DCHECK(parent_window);
  SetOwnedByWidget(true);
  views::DialogDelegate::SetButtons(ui::DIALOG_BUTTON_OK);
  views::DialogDelegate::SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_EXTERNAL_PROTOCOL_CLOSE_BUTTON_TEXT));

  message_box_view_ = new views::MessageBoxView();
  message_box_view_->SetMessageWidth(kMessageWidth);

  views::DialogDelegate::CreateDialogWidget(this, nullptr, parent_window)
      ->Show();
}

ExternalProtocolNoHandlersTelSchemeDialog::
    ~ExternalProtocolNoHandlersTelSchemeDialog() = default;

std::u16string ExternalProtocolNoHandlersTelSchemeDialog::GetWindowTitle()
    const {
  // We display a message to the user on how to use the Click to Call feature.
  return l10n_util::GetStringUTF16(
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_HELP_TEXT_NO_DEVICES);
}

views::View* ExternalProtocolNoHandlersTelSchemeDialog::GetContentsView() {
  return message_box_view_;
}

const views::Widget* ExternalProtocolNoHandlersTelSchemeDialog::GetWidget()
    const {
  return message_box_view_->GetWidget();
}

views::Widget* ExternalProtocolNoHandlersTelSchemeDialog::GetWidget() {
  return message_box_view_->GetWidget();
}

}  // namespace ash
