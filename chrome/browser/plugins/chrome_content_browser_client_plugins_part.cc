// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/chrome_content_browser_client_plugins_part.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/plugins/plugin_info_host_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pepper_permission_util.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/socket_permission.h"
#endif

#if BUILDFLAG(ENABLE_PPAPI)
#include "chrome/browser/renderer_host/pepper/chrome_browser_pepper_host_factory.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/shared_impl/ppapi_switches.h"
#endif  // BUILDFLAG(ENABLE_PPAPI)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/common/webui_url_constants.h"
#endif

namespace plugins {
namespace {

// TODO(teravest): Add renderer-side API-specific checking for these APIs so
// that blanket permission isn't granted to all dev channel APIs for these.
// http://crbug.com/386743

// Set of origins that can use "dev channel" APIs from NaCl, even on stable
// versions of Chrome.
const std::set<std::string>& GetAllowedDevChannelOrigins() {
  static const char* const kPredefinedAllowedDevChannelOrigins[] = {
      "6EAED1924DB611B6EEF2A664BD077BE7EAD33B8F",  // see crbug.com/383937
      "4EB74897CB187C7633357C2FE832E0AD6A44883A"   // see crbug.com/383937
  };
  static base::NoDestructor<std::set<std::string>> origins(
      std::begin(kPredefinedAllowedDevChannelOrigins),
      std::end(kPredefinedAllowedDevChannelOrigins));
  return *origins;
}

// Set of origins that can get a handle for FileIO from NaCl.
const std::set<std::string>& GetAllowedFileHandleOrigins() {
  static const char* const kPredefinedAllowedFileHandleOrigins[] = {
      "6EAED1924DB611B6EEF2A664BD077BE7EAD33B8F",  // see crbug.com/234789
      "4EB74897CB187C7633357C2FE832E0AD6A44883A"   // see crbug.com/234789
  };
  static base::NoDestructor<std::set<std::string>> origins(
      std::begin(kPredefinedAllowedFileHandleOrigins),
      std::end(kPredefinedAllowedFileHandleOrigins));
  return *origins;
}

// Set of origins that can use TCP/UDP private APIs from NaCl.
const std::set<std::string>& GetAllowedSocketOrigins() {
  static const char* const kPredefinedAllowedSocketOrigins[] = {
      "okddffdblfhhnmhodogpojmfkjmhinfp",  // Secure Shell App (dev)
      "pnhechapfaindjhompbnflcldabbghjo",  // Secure Shell App (stable)
      "algkcnfjnajfhgimadimbjhmpaeohhln",  // Secure Shell Extension (dev)
      "iodihamcpbpeioajjeobimgagajmlibd",  // Secure Shell Extension (stable)
      "bglhmjfplikpjnfoegeomebmfnkjomhe",  // see crbug.com/122126
      "cbkkbcmdlboombapidmoeolnmdacpkch",  // see crbug.com/129089
      "hhnbmknkdabfoieppbbljkhkfjcmcbjh",  // see crbug.com/134099
      "mablfbjkhmhkmefkjjacnbaikjkipphg",  // see crbug.com/134099
      "pdeelgamlgannhelgoegilelnnojegoh",  // see crbug.com/134099
      "cabapfdbkniadpollkckdnedaanlciaj",  // see crbug.com/134099
      "mapljbgnjledlpdmlchihnmeclmefbba",  // see crbug.com/134099
      "ghbfeebgmiidnnmeobbbaiamklmpbpii",  // see crbug.com/134099
      "jdfhpkjeckflbbleddjlpimecpbjdeep",  // see crbug.com/142514
      "iabmpiboiopbgfabjmgeedhcmjenhbla",  // see crbug.com/165080
      "B7CF8A292249681AF81771650BA4CEEAF19A4560",  // see crbug.com/165080
      "7525AF4F66763A70A883C4700529F647B470E4D2",  // see crbug.com/238084
      "0B549507088E1564D672F7942EB87CA4DAD73972",  // see crbug.com/238084
      "864288364E239573E777D3E0E36864E590E95C74"   // see crbug.com/238084
  };
  static base::NoDestructor<std::set<std::string>> origins(
      std::begin(kPredefinedAllowedSocketOrigins),
      std::end(kPredefinedAllowedSocketOrigins));
  return *origins;
}

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
                          render_frame_host.GetProcess()->GetID()));
}

bool ChromeContentBrowserClientPluginsPart::
    IsPluginAllowedToCallRequestOSFileHandle(
        content::BrowserContext* browser_context,
        const GURL& url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  Profile* profile = Profile::FromBrowserContext(browser_context);
  const extensions::ExtensionSet* extension_set = nullptr;
  if (profile) {
    extension_set =
        &extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  }

  return IsExtensionOrSharedModuleAllowed(url, extension_set,
                                          GetAllowedFileHandleOrigins()) ||
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
    const content::SocketPermissionRequest* params) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  Profile* profile = Profile::FromBrowserContext(browser_context);
  const extensions::ExtensionSet* extension_set = nullptr;
  if (profile) {
    extension_set =
        &extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  }

  if (private_api) {
    // Access to private socket APIs is controlled by the allowlist.
    if (IsExtensionOrSharedModuleAllowed(url, extension_set,
                                         GetAllowedSocketOrigins())) {
      return true;
    }
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Terminal SWA is not an extension, but runs SSH NaCL with sockets.
    if (url == chrome::kChromeUIUntrustedTerminalURL) {
      return profile->GetPrefs()
          ->FindPreference(crostini::prefs::kTerminalSshAllowedByPolicy)
          ->GetValue()
          ->GetBool();
    }
#endif
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
                  extensions::mojom::APIPermissionID::kSocket, &check_params)) {
            return true;
          }
        } else if (permissions_data->HasAPIPermission(
                       extensions::mojom::APIPermissionID::kSocket)) {
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
              extensions::mojom::APIPermissionID::kVpnProvider)) {
        return true;
      }
    }
  }
#endif

  return false;
}

bool ChromeContentBrowserClientPluginsPart::IsPluginAllowedToUseDevChannelAPIs(
    content::BrowserContext* browser_context,
    const GURL& url) {
#if BUILDFLAG(ENABLE_PPAPI)
  // Allow access for tests.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePepperTesting)) {
    return true;
  }
#endif  // BUILDFLAG(ENABLE_PPAPI)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  Profile* profile = Profile::FromBrowserContext(browser_context);
  const extensions::ExtensionSet* extension_set = nullptr;
  if (profile) {
    extension_set =
        &extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  }

  // Allow access for allowlisted applications.
  if (IsExtensionOrSharedModuleAllowed(url, extension_set,
                                       GetAllowedDevChannelOrigins())) {
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
#if BUILDFLAG(ENABLE_PPAPI)
  browser_host->GetPpapiHost()->AddHostFactoryFilter(
      std::unique_ptr<ppapi::host::HostFactory>(
          new ChromeBrowserPepperHostFactory(browser_host)));
#else
  NOTREACHED_IN_MIGRATION();
#endif  // BUILDFLAG(ENABLE_PPAPI)
}

}  // namespace plugins
