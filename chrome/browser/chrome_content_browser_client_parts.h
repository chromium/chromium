// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_PARTS_H_
#define CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_PARTS_H_

#include <string>
#include <vector>

#include "components/download/public/common/quarantine_connection.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "storage/browser/file_system/file_system_context.h"

namespace base {
class CommandLine;
class FilePath;
}

namespace blink {
namespace web_pref {
struct WebPreferences;
}  // namespace web_pref
class AssociatedInterfaceRegistry;
}

namespace content {
class BrowserContext;
class BrowserURLHandler;
class RenderFrameHost;
class RenderProcessHost;
struct ServiceWorkerVersionBaseInfo;
class SiteInstance;
class WebContents;
}

namespace storage {
class FileSystemBackend;
}

// Implements a platform or feature specific part of ChromeContentBrowserClient.
// All the public methods corresponds to the methods of the same name in
// content::ContentBrowserClient.
class ChromeContentBrowserClientParts {
 public:
  virtual ~ChromeContentBrowserClientParts() {}

  virtual void RenderProcessWillLaunch(content::RenderProcessHost* host) {}
  virtual void SiteInstanceGotProcessAndSite(
      content::SiteInstance* site_instance) {}

  // Subclasses that override webkit preferences are responsible for ensuring
  // that their modifications are mututally exclusive.
  // This is called at startup, and when the user changes their webkit
  // preferences.
  virtual void OverrideWebkitPrefs(content::WebContents* web_contents,
                                   blink::web_pref::WebPreferences* web_prefs) {
  }
  // This is called after each navigation. Return |true| if any changes were
  // made. A response value of |true| will result in IPC to the renderer.
  virtual bool OverrideWebPreferencesAfterNavigation(
      content::WebContents* web_contents,
      blink::web_pref::WebPreferences* web_prefs);

  virtual void BrowserURLHandlerCreated(content::BrowserURLHandler* handler) {}
  virtual void GetAdditionalAllowedSchemesForFileSystem(
      std::vector<std::string>* additional_allowed_schemes) {}
  virtual void GetURLRequestAutoMountHandlers(
      std::vector<storage::URLRequestAutoMountHandler>* handlers) {}
  virtual void GetAdditionalFileSystemBackends(
      content::BrowserContext* browser_context,
      const base::FilePath& storage_partition_path,
      download::QuarantineConnectionCallback quarantine_connection_callback,
      std::vector<std::unique_ptr<storage::FileSystemBackend>>*
          additional_backends) {}

  // Append extra switches to |command_line| for |process|.
  virtual void AppendExtraRendererCommandLineSwitches(
      base::CommandLine* command_line,
      content::RenderProcessHost& process) {}

  // Allows to register browser interfaces exposed through the
  // RenderProcessHost. Note that interface factory callbacks added to
  // |registry| will by default be run immediately on the IO thread, unless a
  // task runner is provided.
  virtual void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      blink::AssociatedInterfaceRegistry* associated_registry,
      content::RenderProcessHost* render_process_host) {}

  // Allows to register browser interfaces exposed to a ServiceWorker.
  virtual void ExposeInterfacesToRendererForServiceWorker(
      const content::ServiceWorkerVersionBaseInfo& service_worker_version_info,
      blink::AssociatedInterfaceRegistry& associated_registry) {}

  // Allows to register browser interfaces exposed to a RenderFrameHost.
  virtual void ExposeInterfacesToRendererForRenderFrameHost(
      content::RenderFrameHost& frame_host,
      blink::AssociatedInterfaceRegistry& associated_registry) {}
};

#endif  // CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_PARTS_H_
