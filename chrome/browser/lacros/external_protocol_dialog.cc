// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/external_protocol/external_protocol_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/arc/arc_external_protocol_dialog.h"
#include "chrome/browser/lacros/arc/arc_intent_helper_mojo_lacros.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/views/external_protocol_dialog.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "url/gurl.h"

using content::WebContents;

namespace {

void OnArcHandled(bool handled) {
  if (handled)
    return;

  // TODO(crbug.com/1293604): Handle dialog more precisely when it is not
  // successfully handled by ARC.
  LOG(WARNING) << "Url is not successfully handled by ARC.";
  return;
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
    const absl::optional<url::Origin>& initiating_origin,
    content::WeakDocumentPtr initiator_document) {
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
      std::make_unique<arc::ArcIntentHelperMojoLacros>(),
      base::BindOnce(&OnArcHandled));
}
