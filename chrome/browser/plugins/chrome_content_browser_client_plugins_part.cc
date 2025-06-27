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
#include "build/build_config.h"
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

#if BUILDFLAG(IS_CHROMEOS)
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

bool ChromeContentBrowserClientPluginsPart::IsPluginAllowedToUseDevChannelAPIs(
    content::BrowserContext* browser_context,
    const GURL& url) {
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

}  // namespace plugins
