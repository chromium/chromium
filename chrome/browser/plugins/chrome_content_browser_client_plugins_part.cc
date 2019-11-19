// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/chrome_content_browser_client_plugins_part.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/task/post_task.h"
#include "chrome/browser/plugins/plugin_info_host_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/pepper/chrome_browser_pepper_host_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pepper_permission_util.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/shared_impl/ppapi_switches.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/socket_permission.h"
#endif

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

ChromeContentBrowserClientPluginsPart::ChromeContentBrowserClientPluginsPart() {
}

ChromeContentBrowserClientPluginsPart::
    ~ChromeContentBrowserClientPluginsPart() {
}

void ChromeContentBrowserClientPluginsPart::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    content::RenderProcessHost* host) {
  associated_registry->AddInterface(
      base::BindRepeating(&BindPluginInfoHost, host->GetID()));
}

bool ChromeContentBrowserClientPluginsPart::
    IsPluginAllowedToCallRequestOSFileHandle(
        content::BrowserContext* browser_context,
        const GURL& url,
        const std::set<std::string>& allowed_file_handle_origins) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  Profile* profile = Profile::FromBrowserContext(browser_context);
  const extensions::ExtensionSet* extension_set = NULL;
  if (profile) {
    extension_set =
        &extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  }

  return IsExtensionOrSharedModuleWhitelisted(url, extension_set,
                                              allowed_file_handle_origins) ||
         IsHostAllowedByCommandLine(url, extension_set,
                                    ::switches::kAllowNaClFileHandleAPI);
#else
  return false;
#endif
}

bool ChromeContentBrowserClientPluginsPart::AllowPepperSocketAPI(
    content::BrowserContext* browser_context,
    const GURL& url,
    bool private_api,
    const content::SocketPermissionRequest* params,
    const std::set<std::string>& allowed_socket_origin) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  Profile* profile = Profile::FromBrowserContext(browser_context);
  const extensions::ExtensionSet* extension_set = NULL;
  if (profile) {
    extension_set =
        &extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  }

  if (private_api) {
    // Access to private socket APIs is controlled by the whitelist.
    if (IsExtensionOrSharedModuleWhitelisted(url, extension_set,
                                             allowed_socket_origin)) {
      return true;
    }
  } else {
    // Access to public socket APIs is controlled by extension permissions.
    if (url.is_valid() && url.SchemeIs(extensions::kExtensionScheme) &&
        extension_set) {
      const extensions::Extension* extension =
          extension_set->GetByID(url.host());
      if (extension) {
        const extensions::PermissionsData* permissions_data =
            extension->permissions_data();
        if (params) {
          extensions::SocketPermission::CheckParam check_params(
              params->type, params->host, params->port);
          if (permissions_data->CheckAPIPermissionWithParam(
                  extensions::APIPermission::kSocket, &check_params)) {
            return true;
          }
        } else if (permissions_data->HasAPIPermission(
                       extensions::APIPermission::kSocket)) {
          return true;
        }
      }
    }
  }

  // Allow both public and private APIs if the command line says so.
  return IsHostAllowedByCommandLine(url, extension_set,
                                    ::switches::kAllowNaClSocketAPI);
#else
  return false;
#endif
}

bool ChromeContentBrowserClientPluginsPart::IsPepperVpnProviderAPIAllowed(
    content::BrowserContext* browser_context,
    const GURL& url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return false;

  const extensions::ExtensionSet* extension_set =
      &extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  if (!extension_set)
    return false;

  // Access to the vpnProvider API is controlled by extension permissions.
  if (url.is_valid() && url.SchemeIs(extensions::kExtensionScheme)) {
    const extensions::Extension* extension = extension_set->GetByID(url.host());
    if (extension) {
      if (extension->permissions_data()->HasAPIPermission(
              extensions::APIPermission::kVpnProvider)) {
        return true;
      }
    }
  }
#endif

  return false;
}

bool ChromeContentBrowserClientPluginsPart::IsPluginAllowedToUseDevChannelAPIs(
    content::BrowserContext* browser_context,
    const GURL& url,
    const std::set<std::string>& allowed_dev_channel_origins) {
  // Allow access for tests.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePepperTesting)) {
    return true;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  Profile* profile = Profile::FromBrowserContext(browser_context);
  const extensions::ExtensionSet* extension_set = NULL;
  if (profile) {
    extension_set =
        &extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  }

  // Allow access for whitelisted applications.
  if (IsExtensionOrSharedModuleWhitelisted(url, extension_set,
                                           allowed_dev_channel_origins)) {
    return true;
  }
#endif
  version_info::Channel channel = chrome::GetChannel();
  // Allow dev channel APIs to be used on "Canary", "Dev", and "Unknown"
  // releases of Chrome. Permitting "Unknown" allows these APIs to be used on
  // Chromium builds as well.
  return channel <= version_info::Channel::DEV;
}

void ChromeContentBrowserClientPluginsPart::DidCreatePpapiPlugin(
    content::BrowserPpapiHost* browser_host) {
  browser_host->GetPpapiHost()->AddHostFactoryFilter(
      std::unique_ptr<ppapi::host::HostFactory>(
          new ChromeBrowserPepperHostFactory(browser_host)));
}

}  // namespace plugins
