// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/devtools_util.h"

#include "base/functional/bind.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/lazy_context_task_queue.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/service_worker/service_worker_task_queue.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"

namespace extensions {
namespace devtools_util {

namespace {

// Helper to inspect an ExtensionHost after it has been loaded.
void InspectExtensionHost(
    DevToolsOpenedByAction opened_by,
    std::unique_ptr<LazyContextTaskQueue::ContextInfo> context_info) {
  if (context_info != nullptr)
    DevToolsWindow::OpenDevToolsWindow(context_info->web_contents, opened_by);
}

void InspectServiceWorkerBackgroundHelper(
    DevToolsOpenedByAction opened_by,
    std::unique_ptr<LazyContextTaskQueue::ContextInfo> context_info) {
  if (!context_info)
    return;

  Profile* profile = Profile::FromBrowserContext(context_info->browser_context);
  const Extension* extension =
      ExtensionRegistry::Get(context_info->browser_context)
          ->enabled_extensions()
          .GetByID(context_info->extension_id);

  // A non-null context info does not guarantee that the extension is enabled,
  // due to thread/process asynchrony.
  if (extension)
    InspectServiceWorkerBackground(extension, profile, opened_by);
}

}  // namespace

// Helper to inspect a service worker after it has been started.
void InspectServiceWorkerBackground(const Extension* extension,
                                    Profile* profile,
                                    DevToolsOpenedByAction opened_by) {
  DCHECK(BackgroundInfo::IsServiceWorkerBased(extension));
  content::DevToolsAgentHost::List targets =
      content::DevToolsAgentHost::GetOrCreateAll();
  for (const scoped_refptr<content::DevToolsAgentHost>& host : targets) {
    if (host->GetType() == content::DevToolsAgentHost::kTypeServiceWorker &&
        host->GetURL() ==
            extension->GetResourceURL(
                BackgroundInfo::GetBackgroundServiceWorkerScript(extension)) &&
        host->GetBrowserContext() == profile) {
      DevToolsWindow::OpenDevToolsWindow(host, profile, opened_by);
      break;
    }
  }
}

void InspectInactiveServiceWorkerBackground(const Extension* extension,
                                            Profile* profile,
                                            DevToolsOpenedByAction opened_by) {
  DCHECK(extension);
  DCHECK(BackgroundInfo::IsServiceWorkerBased(extension));
  const auto context_id = LazyContextId::ForExtension(profile, extension);
  context_id.GetTaskQueue()->AddPendingTask(
      context_id,
      base::BindOnce(&InspectServiceWorkerBackgroundHelper, opened_by));
}

void InspectBackgroundPage(const Extension* extension,
                           Profile* profile,
                           DevToolsOpenedByAction opened_by) {
  DCHECK(extension);
  ExtensionHost* host = ProcessManager::Get(profile)
                            ->GetBackgroundHostForExtension(extension->id());
  if (host) {
    InspectExtensionHost(
        opened_by, std::make_unique<LazyContextTaskQueue::ContextInfo>(host));
  } else {
    const auto context_id = LazyContextId::ForExtension(profile, extension);
    context_id.GetTaskQueue()->AddPendingTask(
        context_id, base::BindOnce(&InspectExtensionHost, opened_by));
  }
}

}  // namespace devtools_util
}  // namespace extensions
