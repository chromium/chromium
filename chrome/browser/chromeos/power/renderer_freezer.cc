// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/renderer_freezer.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"

namespace chromeos {

RendererFreezer::RendererFreezer(
    std::unique_ptr<RendererFreezer::Delegate> delegate)
    : delegate_(std::move(delegate)) {
  delegate_->CheckCanFreezeRenderers(
      base::Bind(&RendererFreezer::OnCheckCanFreezeRenderersComplete,
                 weak_factory_.GetWeakPtr()));
}

RendererFreezer::~RendererFreezer() {
  for (int rph_id : gcm_extension_processes_) {
    content::RenderProcessHost* host =
        content::RenderProcessHost::FromID(rph_id);
    if (host)
      host->RemoveObserver(this);
  }
}

void RendererFreezer::SuspendImminent() {
  // All the delegate's operations are asynchronous so they may not complete
  // before the system suspends.  This is ok since the renderers only need to be
  // frozen in dark resume.  As long as they do get frozen soon after we enter
  // dark resume, there shouldn't be a problem.
  delegate_->FreezeRenderers();
}

void RendererFreezer::SuspendDone() {
  delegate_->ThawRenderers(base::Bind(&RendererFreezer::OnThawRenderersComplete,
                                      weak_factory_.GetWeakPtr()));
}

void RendererFreezer::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CREATED: {
      content::RenderProcessHost* process =
          content::Source<content::RenderProcessHost>(source).ptr();
      OnRenderProcessCreated(process);
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

void RendererFreezer::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  auto it = gcm_extension_processes_.find(host->GetID());
  if (it == gcm_extension_processes_.end()) {
    LOG(ERROR) << "Received unrequested RenderProcessExited message";
    return;
  }
  gcm_extension_processes_.erase(it);

  // When this function is called, the renderer process has died but the
  // RenderProcessHost will not be destroyed.  If a new renderer process is
  // created for this RPH, registering as an observer again will trigger a
  // warning about duplicate observers.  To prevent this we just stop observing
  // this RPH until another renderer process is created for it.
  host->RemoveObserver(this);
}

void RendererFreezer::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  auto it = gcm_extension_processes_.find(host->GetID());
  if (it == gcm_extension_processes_.end()) {
    LOG(ERROR) << "Received unrequested RenderProcessHostDestroyed message";
    return;
  }

  gcm_extension_processes_.erase(it);
}

void RendererFreezer::OnCheckCanFreezeRenderersComplete(bool can_freeze) {
  if (!can_freeze)
    return;

  PowerManagerClient::Get()->SetRenderProcessManagerDelegate(
      weak_factory_.GetWeakPtr());

  registrar_.Add(
      this,
      content::NOTIFICATION_RENDERER_PROCESS_CREATED,
      content::NotificationService::AllBrowserContextsAndSources());
}

void RendererFreezer::OnThawRenderersComplete(bool success) {
  if (success)
    return;

  // We failed to write the thaw command and the renderers are still frozen.  We
  // are in big trouble because none of the tabs will be responsive so let's
  // crash the browser instead.
  LOG(FATAL) << "Unable to thaw renderers.";
}

void RendererFreezer::OnRenderProcessCreated(content::RenderProcessHost* rph) {
  const int rph_id = rph->GetID();

  if (gcm_extension_processes_.find(rph_id) != gcm_extension_processes_.end()) {
    LOG(ERROR) << "Received duplicate notifications about the creation of a "
               << "RenderProcessHost with id " << rph_id;
    return;
  }

  // According to extensions::ProcessMap, extensions and renderers have a
  // many-to-many relationship.  Specifically, a hosted app can appear in many
  // renderers while any other kind of extension can be running in "split mode"
  // if there is an incognito window open and so could appear in two renderers.
  //
  // We don't care about hosted apps because they cannot use GCM so we only need
  // to worry about extensions in "split mode".  Luckily for us this function is
  // called any time a new renderer process is created so we don't really need
  // to care whether we are currently in an incognito context.  We just need to
  // iterate over all the extensions in the newly created process and take the
  // appropriate action based on whether we find an extension using GCM.
  content::BrowserContext* context = rph->GetBrowserContext();
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(context);
  for (const std::string& extension_id :
       extensions::ProcessMap::Get(context)->GetExtensionsInProcess(rph_id)) {
    const extensions::Extension* extension = registry->GetExtensionById(
        extension_id, extensions::ExtensionRegistry::ENABLED);
    if (!extension ||
        !extension->permissions_data()->HasAPIPermission(
            extensions::APIPermission::kGcm)) {
      continue;
    }

    // This renderer has an extension that is using GCM.  Make sure it is not
    // frozen during suspend.
    delegate_->SetShouldFreezeRenderer(rph->GetProcess().Handle(), false);
    gcm_extension_processes_.insert(rph_id);

    // Watch to see if the renderer process or the RenderProcessHost is
    // destroyed.
    rph->AddObserver(this);
    return;
  }

  // We didn't find an extension in this RenderProcessHost that is using GCM so
  // we can go ahead and freeze it on suspend.
  delegate_->SetShouldFreezeRenderer(rph->GetProcess().Handle(), true);
}

}  // namespace chromeos
