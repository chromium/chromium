// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pwc/pwc_api_binder.h"

#include <utility>

#include "chrome/browser/bad_message.h"
#include "chrome/browser/pwc/privileged_web_contents.h"
#include "chrome/browser/pwc/pwc_component_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/security_principal.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace pwc {

PwcApiBinder::PwcApiBinder() = default;

PwcApiBinder::~PwcApiBinder() = default;

void PwcApiBinder::Bind(
    mojo::PendingReceiver<mojom::PrivilegedBridge> receiver) {
  receivers_.Add(this, std::move(receiver));
}

namespace {

// Tier 1 of the capability gate -- static properties of the requesting
// document that a well-behaved renderer can never get wrong: it must belong
// to a PrivilegedWebContents, be an outermost main frame (never a subframe
// or a fenced frame), and have committed over HTTPS to a capability origin.
// The HTTPS-URL check (not just an HTTPS *origin*) is what rejects
// inheriting schemes -- about:blank/srcdoc, blob:, data:, filesystem: --
// whose document can carry an inherited capability origin while never
// actually loading the trusted resource. None of these can change for the
// lifetime of the document, so a violation cannot be an innocent race.
bool IsStructurallyQualified(content::RenderFrameHost* render_frame_host,
                             PrivilegedWebContents* privileged) {
  return privileged && !render_frame_host->GetParentOrOuterDocument() &&
         render_frame_host->GetLastCommittedURL().SchemeIs(url::kHttpsScheme) &&
         privileged->policy().IsCapabilityOrigin(
             render_frame_host->GetLastCommittedOrigin());
}

// Tier 2 of the capability gate -- conditions a well-behaved renderer cannot
// control or observe race-free:
// - The document must currently be the primary main frame. A legitimate
//   request can be in flight while the browser commits a cross-document
//   navigation that moves the requesting document out of the primary page
//   (pending deletion). (A privileged WebContents never hosts prerendered or
//   back-forward-cached pages, so pending deletion is the only such state.)
//   Checking IsInPrimaryMainFrame() rather than IsOutermostMainFrame() also
//   keeps the gate correct if the PWC is ever embedded.
// - The document must actually run in an origin-keyed process: in a merely
//   site-keyed privileged process, a same-site cross-origin subframe could
//   share the qualifying frame's process, so such a process must not hold
//   capabilities.
bool IsLifecycleAndIsolationQualified(
    content::RenderFrameHost* render_frame_host) {
  return render_frame_host->IsInPrimaryMainFrame() &&
         render_frame_host->GetSiteInstance()
             ->GetSecurityPrincipal()
             .IsOriginKeyed();
}

}  // namespace

bool IsCapabilityQualifiedFrame(content::RenderFrameHost* render_frame_host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  PrivilegedWebContents* privileged =
      PrivilegedWebContents::FromWebContents(web_contents);
  return IsStructurallyQualified(render_frame_host, privileged) &&
         IsLifecycleAndIsolationQualified(render_frame_host);
}

PrivilegedWebContents* EnforceCapabilityGate(
    content::RenderFrameHost* render_frame_host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  PrivilegedWebContents* privileged =
      PrivilegedWebContents::FromWebContents(web_contents);

  // Fail closed, in two tiers. A tier-1 (structural) violation is a
  // compromised renderer and terminates the process; a tier-2 failure is not
  // renderer-controllable, so the receiver is dropped without a kill.
  if (!IsStructurallyQualified(render_frame_host, privileged)) {
    bad_message::ReceivedBadMessage(render_frame_host->GetProcess(),
                                    bad_message::PWC_BRIDGE_UNQUALIFIED_FRAME);
    return nullptr;
  }

  if (!IsLifecycleAndIsolationQualified(render_frame_host)) {
    return nullptr;
  }

  return privileged;
}

void BindPrivilegedBridge(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::PrivilegedBridge> receiver) {
  PrivilegedWebContents* privileged = EnforceCapabilityGate(render_frame_host);
  if (!privileged) {
    return;
  }

  privileged->bridge().Bind(std::move(receiver));
}

}  // namespace pwc
