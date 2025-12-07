// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/chrome_content_browser_client_plugins_part.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/plugins/plugin_info_host_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace plugins {
namespace {

void BindPluginInfoHost(
    int render_process_id,
    mojo::PendingAssociatedReceiver<chrome::mojom::PluginInfoHost> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id);
  if (!host)
    return;

  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());
  mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<PluginInfoHostImpl>(render_process_id, profile),
      std::move(receiver));
}

}  // namespace

ChromeContentBrowserClientPluginsPart::ChromeContentBrowserClientPluginsPart() =
    default;

ChromeContentBrowserClientPluginsPart::
    ~ChromeContentBrowserClientPluginsPart() = default;

void ChromeContentBrowserClientPluginsPart::
    ExposeInterfacesToRendererForRenderFrameHost(
        content::RenderFrameHost& render_frame_host,
        blink::AssociatedInterfaceRegistry& associated_registry) {
  associated_registry.AddInterface<chrome::mojom::PluginInfoHost>(
      base::BindRepeating(&BindPluginInfoHost,
                          render_frame_host.GetProcess()->GetDeprecatedID()));
}

}  // namespace plugins
