// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PWC_PWC_API_BINDER_H_
#define CHROME_BROWSER_PWC_PWC_API_BINDER_H_

#include "chrome/browser/pwc/pwc.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace pwc {

class PrivilegedWebContents;

// The single shared security gate for every privileged capability binder --
// the bridge itself and per-component APIs such as //chrome/browser/geic.
// Returns the PrivilegedWebContents whose privileged capability surface
// `render_frame_host` is entitled to, or null if the request must be
// rejected. Enforcement is folded in here, in two tiers, so a binder cannot
// apply the gate partially:
//
// - A violation of a static property of the requesting document -- not an
//   outermost main frame of a PrivilegedWebContents, or not committed over
//   HTTPS to a capability origin -- could only come from a compromised
//   renderer; this reports a bad message (terminating the renderer) and
//   returns null.
// - A condition the renderer cannot control or observe race-free -- the
//   document no longer being the primary main frame (pending deletion), or
//   its process lacking origin isolation -- also yields null, but with no
//   kill: the receiver is simply dropped.
//
// Either way, on null the caller just returns, dropping the receiver. On
// non-null the caller may bind, after any component-specific checks of its
// own (a component mismatch is again a compromised renderer; see
// BindGeicApi).
PrivilegedWebContents* EnforceCapabilityGate(
    content::RenderFrameHost* render_frame_host);

// Pure predicate form of the capability gate: returns true iff
// `render_frame_host` would pass EnforceCapabilityGate() in full -- the
// primary main frame of a PrivilegedWebContents, committed over HTTPS to a
// capability origin, running in an origin-keyed process. Unlike the gate it
// has NO side effects (no bad_message, no receiver handling), so it is safe
// for embedder hooks that merely need to know whether a frame is the
// qualifying privileged frame -- e.g. deciding whether MojoJS bindings may
// be enabled for a document. Binders must use EnforceCapabilityGate()
// instead, so that a compromised-renderer request is terminated.
bool IsCapabilityQualifiedFrame(content::RenderFrameHost* render_frame_host);

// Browser-side host for the privileged capability bridge
// (pwc::mojom::PrivilegedBridge). Owned by its PrivilegedWebContents and lives
// for the WebContents' lifetime.
//
// Receiver lifetime is deliberately coarse -- receivers are dropped when the
// WebContents goes away, not when the binding document does. This is safe
// because mere possession of a bound pipe grants nothing:
// - The interface currently has no methods. When real capability methods
//   land, they must either re-validate the caller per call or move this host
//   to a document-scoped abstraction (content::DocumentService) so a stale
//   pipe cannot outlive the document that qualified for it.
// - The bind-time gate (BindPrivilegedBridge) re-checks full qualification on
//   every bind, and the navigation throttle locks the primary main frame to
//   allowlisted origins, so no frame can acquire a pipe it does not currently
//   qualify for.
// - A renderer that artificially keeps a pipe alive across a cross-origin
//   navigation cannot keep it past its process: privileged documents run in
//   origin-keyed processes, so the pipe's renderer end dies with the old
//   origin's process, and a same-origin successor document would re-qualify
//   anyway.
// Note that a renderer holding this bridge is *not* trusted like the browser
// process: it is an ordinary web renderer whose extra capabilities are
// individually brokered and re-gated here.
class PwcApiBinder : public mojom::PrivilegedBridge {
 public:
  PwcApiBinder();
  ~PwcApiBinder() override;

  PwcApiBinder(const PwcApiBinder&) = delete;
  PwcApiBinder& operator=(const PwcApiBinder&) = delete;

  // Binds `receiver` to this host. The caller (BindPrivilegedBridge) has
  // already verified the requesting frame fully qualifies.
  void Bind(mojo::PendingReceiver<mojom::PrivilegedBridge> receiver);

 private:
  mojo::ReceiverSet<mojom::PrivilegedBridge> receivers_;
};

// Frame binder registered in PopulateChromeFrameBinders for
// pwc::mojom::PrivilegedBridge.
//
// Security gate, enforced in two tiers (see the implementation for details):
// a violation of a static property of the requesting document -- not an
// outermost main frame of a PrivilegedWebContents, or not committed over
// HTTPS to a capability origin -- is a compromised renderer and terminates
// the process via bad_message, while a condition the renderer cannot control
// or observe race-free -- the document no longer being the primary main frame
// (pending deletion), or its process lacking origin isolation -- silently
// drops the receiver.
void BindPrivilegedBridge(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::PrivilegedBridge> receiver);

}  // namespace pwc

#endif  // CHROME_BROWSER_PWC_PWC_API_BINDER_H_
