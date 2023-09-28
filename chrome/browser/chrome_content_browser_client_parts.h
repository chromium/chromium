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
class RenderProcessHost;
struct ServiceWorkerVersionBaseInfo;
class SiteInstance;
class WebContents;
}

namespace storage {
class FileSystemBackend;
}

class Profile;

// Implements a platform or feature specific part of ChromeContentBrowserClient.
// All the public methods corresponds to the methods of the same name in
// content::ContentBrowserClient.
class ChromeContentBrowserClientParts {
 public:
  virtual ~ChromeContentBrowserClientParts() {}

  virtual void RenderProcessWillLaunch(content::RenderProcessHost* host) {}
  virtual void SiteInstanceGotProcess(content::SiteInstance* site_instance) {}
  virtual void OverrideWebkitPrefs(content::WebContents* web_contents,
                                   blink::web_pref::WebPreferences* web_prefs) {
  }
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

  // Append extra switches to |command_line| for |process|. If |process| is not
  // NULL, then neither is |profile|.
  virtual void AppendExtraRendererCommandLineSwitches(
      base::CommandLine* command_line,
      content::RenderProcessHost* process,
      Profile* profile) {}

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
};

#endif  // CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_PARTS_H_

