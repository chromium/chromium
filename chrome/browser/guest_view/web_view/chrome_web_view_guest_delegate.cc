// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "chrome/browser/guest_view/web_view/chrome_web_view_guest_delegate.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/ui/pdf/chrome_pdf_web_contents_helper_client.h"
#include "components/guest_view/browser/guest_view_event.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"

using guest_view::GuestViewEvent;

namespace extensions {

ChromeWebViewGuestDelegate::ChromeWebViewGuestDelegate(
    WebViewGuest* web_view_guest)
    : pending_context_menu_request_id_(0), web_view_guest_(web_view_guest) {}

ChromeWebViewGuestDelegate::~ChromeWebViewGuestDelegate() {
}

bool ChromeWebViewGuestDelegate::HandleContextMenu(
    const content::ContextMenuParams& params) {
  if ((params.source_type == ui::MENU_SOURCE_LONG_PRESS ||
       params.source_type == ui::MENU_SOURCE_LONG_TAP ||
       params.source_type == ui::MENU_SOURCE_TOUCH) &&
      !params.selection_text.empty() &&
      (guest_web_contents()->GetRenderWidgetHostView() &&
       guest_web_contents()
           ->GetRenderWidgetHostView()
           ->GetTouchSelectionControllerClientManager())) {
    // This context menu request should be handled by the
    // TouchSelectionController. If the user selects the full context menu from
    // the QuickMenu, the request will come back here (with different source
    // parameters) to complete.
    return true;
  }

  ContextMenuDelegate* menu_delegate =
      ContextMenuDelegate::FromWebContents(guest_web_contents());
  DCHECK(menu_delegate);
  pending_menu_ = menu_delegate->BuildMenu(guest_web_contents(), params);
  // It's possible for the returned menu to be null, so early out to avoid
  // a crash. TODO(wjmaclean): find out why it's possible for this to happen
  // in the first place, and if it's an error.
  if (!pending_menu_)
    return false;

  // Pass it to embedder.
  int request_id = ++pending_context_menu_request_id_;
  std::unique_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  std::unique_ptr<base::ListValue> items =
      MenuModelToValue(pending_menu_->menu_model());
  args->Set(webview::kContextMenuItems, std::move(items));
  args->SetInteger(webview::kRequestId, request_id);
  web_view_guest()->DispatchEventToView(std::make_unique<GuestViewEvent>(
      webview::kEventContextMenuShow, std::move(args)));
  return true;
}

// static
std::unique_ptr<base::ListValue> ChromeWebViewGuestDelegate::MenuModelToValue(
    const ui::SimpleMenuModel& menu_model) {
  std::unique_ptr<base::ListValue> items(new base::ListValue());
  for (int i = 0; i < menu_model.GetItemCount(); ++i) {
    std::unique_ptr<base::DictionaryValue> item_value(
        new base::DictionaryValue());
    // TODO(lazyboy): We need to expose some kind of enum equivalent of
    // |command_id| instead of plain integers.
    item_value->SetInteger(webview::kMenuItemCommandId,
                           menu_model.GetCommandIdAt(i));
    item_value->SetString(webview::kMenuItemLabel, menu_model.GetLabelAt(i));
    items->Append(std::move(item_value));
  }
  return items;
}

void ChromeWebViewGuestDelegate::OnShowContextMenu(int request_id) {
  if (!pending_menu_)
    return;

  // Make sure this was the correct request.
  if (request_id != pending_context_menu_request_id_)
    return;

  // TODO(lazyboy): Implement.

  ContextMenuDelegate* menu_delegate =
      ContextMenuDelegate::FromWebContents(guest_web_contents());
  menu_delegate->ShowMenu(std::move(pending_menu_));
}

}  // namespace extensions
