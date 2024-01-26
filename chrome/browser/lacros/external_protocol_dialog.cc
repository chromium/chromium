// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/external_protocol/external_protocol_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/arc/arc_external_protocol_dialog.h"
#include "chrome/browser/lacros/arc/arc_intent_helper_mojo_lacros.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/views/external_protocol_dialog.h"
#include "chromeos/crosapi/mojom/url_handler.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "url/gurl.h"

using content::WebContents;

namespace {

void OnGetExternalHandler(const GURL& url,
                          const std::optional<url::Origin>& initiating_origin,
                          content::WeakDocumentPtr initiator_document,
                          base::WeakPtr<WebContents> web_contents,
                          const std::optional<std::string>& name) {
  // If WebContents have been destroyed, do not show any dialog.
  if (!web_contents) {
    return;
  }

  aura::Window* parent_window = web_contents->GetTopLevelNativeWindow();
  // If WebContents has been detached from window tree, do not show any dialog.
  if (!parent_window || !parent_window->GetRootWindow()) {
    return;
  }
  if (name) {
    new ExternalProtocolDialog(web_contents.get(), url,
                               base::UTF8ToUTF16(*name), initiating_origin,
                               initiator_document);
  }
}

void OnArcHandled(const GURL& url,
                  const std::optional<url::Origin>& initiating_origin,
                  content::WeakDocumentPtr initiator_document,
                  base::WeakPtr<WebContents> web_contents,
                  bool handled) {
  if (handled) {
    return;
  }

  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (service->GetInterfaceVersion<crosapi::mojom::UrlHandler>() >=
      int{crosapi::mojom::UrlHandler::kGetExternalHandlerMinVersion}) {
    service->GetRemote<crosapi::mojom::UrlHandler>()->GetExternalHandler(
        url,
        base::BindOnce(&OnGetExternalHandler, url, initiating_origin,
                       std::move(initiator_document), std::move(web_contents)));
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
    const std::optional<url::Origin>& initiating_origin,
    content::WeakDocumentPtr initiator_document,
    const std::u16string& program_name) {
  // First, check if ARC version of the dialog is available and run ARC version
  // when possible.
  arc::RunArcExternalProtocolDialog(
      url, initiating_origin, web_contents->GetWeakPtr(), page_transition,
      has_user_gesture, is_in_fenced_frame_tree,
      std::make_unique<arc::ArcIntentHelperMojoLacros>(),
      base::BindOnce(&OnArcHandled, url, initiating_origin,
                     std::move(initiator_document),
                     web_contents->GetWeakPtr()));
}
