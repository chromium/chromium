// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEIC_GEIC_HOST_H_
#define CHROME_BROWSER_GEIC_GEIC_HOST_H_

#include "chrome/browser/geic/geic.mojom.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace pwc {
class PrivilegedWebContents;
}  // namespace pwc

namespace geic {

// All PWC-scoped browser-side GEIC state for a single GEIC
// PrivilegedWebContents. Created and owned by whatever creates the GEIC PWC
// (the GEIC controller), scoped to that PWC's lifetime; it registers itself
// into the PWC's UnownedUserDataHost so any consumer holding the PWC can
// retrieve it via GeicHost::Get(pwc.unowned_user_data_host()).
// PrivilegedWebContents neither owns this nor depends on
// //chrome/browser/geic, so there is no dependency cycle. Scaffolding: the
// GEIC team adds its PWC-scoped state here. Live GeicApi receivers do NOT
// live here -- each is a document-scoped GeicApiBinding, so state that
// belongs to one document never outlives it.
class GeicHost {
 public:
  // Registers this host into `privileged`'s UnownedUserDataHost. `privileged`
  // must outlive this GeicHost (the GEIC controller owns both and destroys this
  // one first).
  explicit GeicHost(pwc::PrivilegedWebContents& privileged);
  ~GeicHost();

  GeicHost(const GeicHost&) = delete;
  GeicHost& operator=(const GeicHost&) = delete;

  // Provides the static GeicHost::Get(UnownedUserDataHost&) lookup and the key.
  DECLARE_USER_DATA(GeicHost);

 private:
  ui::ScopedUnownedUserData<GeicHost> scoped_data_;
};

// Browser-side implementation of geic::mojom::GeicApi, scoped to the bound
// document via content::DocumentService: one self-owned instance per accepted
// bind, destroyed by the browser when its document is destroyed or navigated
// cross-document (or on pipe disconnect). Methods added later reach the
// PWC-scoped state via GeicHost::Get() at call time; anything stored on this
// class is document-scoped by construction.
class GeicApiBinding : public content::DocumentService<mojom::GeicApi> {
 public:
  GeicApiBinding(const GeicApiBinding&) = delete;
  GeicApiBinding& operator=(const GeicApiBinding&) = delete;

  // Creates a self-owned binding bound to `render_frame_host`'s current
  // document. Must only be called by BindGeicApi, after the frame has passed
  // the full gate.
  static void Create(content::RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<mojom::GeicApi> receiver);

 private:
  GeicApiBinding(content::RenderFrameHost& render_frame_host,
                 mojo::PendingReceiver<mojom::GeicApi> receiver);
  ~GeicApiBinding() override;
};

// Frame binder registered in PopulateChromeFrameBinders for
// geic::mojom::GeicApi.
//
// Binds the interface only for a frame that passes the shared capability gate
// (pwc::EnforceCapabilityGate, which itself terminates the renderer on a
// renderer-controllable violation and silently refuses the conditions the
// renderer cannot control) AND whose PrivilegedWebContents serves the GEIC
// component; the receiver becomes a document-scoped GeicApiBinding. A
// qualifying frame of another component's PWC is a compromised renderer
// requesting an interface it was never exposed to, and is terminated via
// bad_message -- the same handling as the capability bridge.
void BindGeicApi(content::RenderFrameHost* render_frame_host,
                 mojo::PendingReceiver<mojom::GeicApi> receiver);

}  // namespace geic

#endif  // CHROME_BROWSER_GEIC_GEIC_HOST_H_
