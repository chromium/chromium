// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geic/geic_host.h"

#include <utility>

#include "chrome/browser/bad_message.h"
#include "chrome/browser/pwc/privileged_web_contents.h"
#include "chrome/browser/pwc/pwc_api_binder.h"
#include "chrome/browser/pwc/pwc_component_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"

namespace geic {

DEFINE_USER_DATA(GeicHost);

GeicHost::GeicHost(pwc::PrivilegedWebContents& privileged)
    : scoped_data_(privileged.unowned_user_data_host(), *this) {}

GeicHost::~GeicHost() = default;

GeicApiBinding::GeicApiBinding(content::RenderFrameHost& render_frame_host,
                               mojo::PendingReceiver<mojom::GeicApi> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

GeicApiBinding::~GeicApiBinding() = default;

// static
void GeicApiBinding::Create(content::RenderFrameHost* render_frame_host,
                            mojo::PendingReceiver<mojom::GeicApi> receiver) {
  // Self-owned; DocumentService deletes it when the bound document is
  // destroyed or navigated cross-document, or when the pipe disconnects.
  new GeicApiBinding(*render_frame_host, std::move(receiver));
}

void BindGeicApi(content::RenderFrameHost* render_frame_host,
                 mojo::PendingReceiver<mojom::GeicApi> receiver) {
  // The shared gate enforces the full capability policy (and terminates the
  // renderer itself on a renderer-controllable violation); null simply means
  // do not bind.
  pwc::PrivilegedWebContents* privileged =
      pwc::EnforceCapabilityGate(render_frame_host);
  if (!privileged) {
    return;
  }

  // The gate is component-agnostic; GeicApi additionally requires the
  // component to be GEIC. A request from another component's frame is again a
  // compromised renderer.
  if (privileged->component() != pwc::PrivilegedComponent::kGeic) {
    bad_message::ReceivedBadMessage(render_frame_host->GetProcess(),
                                    bad_message::PWC_BRIDGE_UNQUALIFIED_FRAME);
    return;
  }

  if (!GeicHost::Get(privileged->unowned_user_data_host())) {
    // The frame qualifies, but the GEIC component has not attached its GeicHost
    // to this PrivilegedWebContents. That is a browser-side setup gap, not a
    // compromised renderer, so drop the request rather than terminating the
    // renderer.
    return;
  }
  GeicApiBinding::Create(render_frame_host, std::move(receiver));
}

}  // namespace geic
