// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_info_host_impl.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/singleton.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/plugin.mojom.h"
#include "components/component_updater/component_updater_service.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/nacl/common/buildflags.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "components/guest_view/browser/guest_view_base.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/webview_info.h"
#endif

#if BUILDFLAG(ENABLE_NACL)
#include "components/nacl/common/nacl_constants.h"
#endif

using content::PluginService;
using content::WebPluginInfo;

namespace {

class PluginInfoHostImplShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static PluginInfoHostImplShutdownNotifierFactory* GetInstance() {
    return base::Singleton<PluginInfoHostImplShutdownNotifierFactory>::get();
  }

  PluginInfoHostImplShutdownNotifierFactory(
      const PluginInfoHostImplShutdownNotifierFactory&) = delete;
  PluginInfoHostImplShutdownNotifierFactory& operator=(
      const PluginInfoHostImplShutdownNotifierFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<
      PluginInfoHostImplShutdownNotifierFactory>;

  PluginInfoHostImplShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "PluginInfoHostImpl") {}

  ~PluginInfoHostImplShutdownNotifierFactory() override {}
};

std::unique_ptr<PluginMetadata> GetPluginMetadata(const WebPluginInfo& plugin) {
  // Gets the base name of the file path as the identifier.
  std::string identifier = plugin.path.BaseName().AsUTF8Unsafe();

  // Gets the plugin group name as the plugin name if it is not empty, or the
  // filename without extension if the name is empty.
  std::u16string group_name = plugin.name;
  if (group_name.empty()) {
    group_name = plugin.path.BaseName().RemoveExtension().AsUTF16Unsafe();
  } else {
    // Remove any unwanted locale direction characters from the group name.
    // For extension-based plugins, the plugin name is derived from the
    // extension name, and `extensions::Extension::LoadName()` may add locale
    // direction characters to the extension name.
    base::i18n::UnadjustStringForLocaleDirection(&group_name);
  }

  // Treat plugins as requiring authorization by default.
  PluginMetadata::SecurityStatus security_status =
      PluginMetadata::SECURITY_STATUS_REQUIRES_AUTHORIZATION;

  // Handle the PDF plugins specially.
  if (plugin.path.value() == ChromeContentClient::kPDFExtensionPluginPath) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    identifier = "google-chrome-pdf";
#else
    identifier = "chromium-pdf";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    security_status = PluginMetadata::SECURITY_STATUS_FULLY_TRUSTED;
  } else if (plugin.path.value() ==
             ChromeContentClient::kPDFInternalPluginPath) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    identifier = "google-chrome-pdf-plugin";
#else
    identifier = "chromium-pdf-plugin";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    security_status = PluginMetadata::SECURITY_STATUS_FULLY_TRUSTED;
  }

  return std::make_unique<PluginMetadata>(identifier, group_name,
                                          security_status);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Returns whether a request from a plugin to load |resource| from a renderer
// with process id |process_id| is a request for an internal resource by an app
// listed in |accessible_resources| in its manifest.
bool IsPluginLoadingAccessibleResourceInWebView(
    extensions::ExtensionRegistry* extension_registry,
    int process_id,
    const GURL& resource) {
  extensions::WebViewRendererState* renderer_state =
      extensions::WebViewRendererState::GetInstance();
  std::string partition_id;
  if (!renderer_state->IsGuest(process_id) ||
      !renderer_state->GetPartitionID(process_id, &partition_id)) {
    return false;
  }

  const std::string extension_id = resource.host();
  const extensions::Extension* extension =
      extension_registry->enabled_extensions().GetByID(extension_id);
  if (!extension || !extensions::WebviewInfo::IsResourceWebviewAccessible(
                        extension, partition_id, resource.path())) {
    return false;
  }

  // Make sure the renderer making the request actually belongs to the
  // same extension.
  std::string owner_extension;
  return renderer_state->GetOwnerInfo(process_id, nullptr, &owner_extension) &&
         owner_extension == extension_id;
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace

PluginInfoHostImpl::Context::Context(int render_process_id, Profile* profile)
    : render_process_id_(render_process_id),
#if BUILDFLAG(ENABLE_EXTENSIONS)
      extension_registry_(extensions::ExtensionRegistry::Get(profile)),
#endif
      host_content_settings_map_(
          HostContentSettingsMapFactory::GetForProfile(profile)),
      plugin_prefs_(PluginPrefs::GetForProfile(profile)) {
}

PluginInfoHostImpl::Context::~Context() = default;

PluginInfoHostImpl::PluginInfoHostImpl(int render_process_id, Profile* profile)
    : context_(render_process_id, profile) {
  shutdown_subscription_ =
      PluginInfoHostImplShutdownNotifierFactory::GetInstance()
          ->Get(profile)
          ->Subscribe(base::BindRepeating(
              &PluginInfoHostImpl::ShutdownOnUIThread, base::Unretained(this)));
}

void PluginInfoHostImpl::ShutdownOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  shutdown_subscription_ = {};
}

PluginInfoHostImpl::~PluginInfoHostImpl() = default;

struct PluginInfoHostImpl::GetPluginInfo_Params {
  GURL url;
  url::Origin main_frame_origin;
  std::string mime_type;
};

void PluginInfoHostImpl::GetPluginInfo(const GURL& url,
                                       const url::Origin& origin,
                                       const std::string& mime_type,
                                       GetPluginInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GetPluginInfo_Params params = {url, origin, mime_type};
  PluginService::GetInstance()->GetPlugins(
      base::BindOnce(&PluginInfoHostImpl::PluginsLoaded,
                     weak_factory_.GetWeakPtr(), params, std::move(callback)));
}

void PluginInfoHostImpl::PluginsLoaded(
    const GetPluginInfo_Params& params,
    GetPluginInfoCallback callback,
    const std::vector<WebPluginInfo>& plugins) {
  chrome::mojom::PluginInfoPtr output = chrome::mojom::PluginInfo::New();
  // This also fills in |actual_mime_type|.
  std::unique_ptr<PluginMetadata> plugin_metadata;
  if (context_.FindEnabledPlugin(params.url, params.mime_type, &output->status,
                                 &output->plugin, &output->actual_mime_type,
                                 &plugin_metadata)) {
    // TODO(crbug.com/40164563): Simplify this once PDF is the only "plugin."
    context_.DecidePluginStatus(params.url, params.main_frame_origin,
                                output->plugin,
                                plugin_metadata->security_status(),
                                plugin_metadata->identifier(), &output->status);
  }

  GetPluginInfoFinish(params, std::move(output), std::move(callback),
                      std::move(plugin_metadata));
}

void PluginInfoHostImpl::Context::DecidePluginStatus(
    const GURL& url,
    const url::Origin& main_frame_origin,
    const WebPluginInfo& plugin,
    PluginMetadata::SecurityStatus security_status,
    const std::string& plugin_identifier,
    chrome::mojom::PluginStatus* status) const {
  if (security_status == PluginMetadata::SECURITY_STATUS_FULLY_TRUSTED) {
    *status = chrome::mojom::PluginStatus::kAllowed;
    return;
  }

  ContentSetting plugin_setting = CONTENT_SETTING_DEFAULT;
  bool uses_default_content_setting = true;
  bool is_managed = false;
  // Check plugin content settings. The primary URL is the top origin URL and
  // the secondary URL is the plugin URL.
  PluginUtils::GetPluginContentSetting(
      host_content_settings_map_, plugin, main_frame_origin, url,
      plugin_identifier, &plugin_setting, &uses_default_content_setting,
      &is_managed);

  DCHECK(plugin_setting != CONTENT_SETTING_DEFAULT);

  // Check if the plugin is crashing too much.
  if (PluginService::GetInstance()->IsPluginUnstable(plugin.path) &&
      plugin_setting != CONTENT_SETTING_BLOCK && uses_default_content_setting) {
    *status = chrome::mojom::PluginStatus::kUnauthorized;
    return;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // If an app has explicitly made internal resources available by listing them
  // in |accessible_resources| in the manifest, then allow them to be loaded by
  // plugins inside a guest-view.
  if (url.SchemeIs(extensions::kExtensionScheme) && !is_managed &&
      plugin_setting == CONTENT_SETTING_BLOCK &&
      IsPluginLoadingAccessibleResourceInWebView(extension_registry_,
                                                 render_process_id_, url)) {
    plugin_setting = CONTENT_SETTING_ALLOW;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  if (plugin_setting == CONTENT_SETTING_ASK ||
      plugin_setting == CONTENT_SETTING_ALLOW) {
    *status = chrome::mojom::PluginStatus::kPlayImportantContent;
  } else if (plugin_setting == CONTENT_SETTING_BLOCK) {
    *status = is_managed ? chrome::mojom::PluginStatus::kBlockedByPolicy
                         : chrome::mojom::PluginStatus::kBlocked;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Allow an embedder of <webview> to block a plugin from being loaded inside
  // the guest. In order to do this, set the status to 'Unauthorized' here,
  // and update the status as appropriate depending on the response from the
  // embedder.
  if (*status == chrome::mojom::PluginStatus::kAllowed ||
      *status == chrome::mojom::PluginStatus::kBlocked ||
      *status == chrome::mojom::PluginStatus::kPlayImportantContent) {
    if (extensions::WebViewRendererState::GetInstance()->IsGuest(
            render_process_id_))
      *status = chrome::mojom::PluginStatus::kUnauthorized;
  }
#endif
}

bool PluginInfoHostImpl::Context::FindEnabledPlugin(
    const GURL& url,
    const std::string& mime_type,
    chrome::mojom::PluginStatus* status,
    WebPluginInfo* plugin,
    std::string* actual_mime_type,
    std::unique_ptr<PluginMetadata>* plugin_metadata) const {
  *status = chrome::mojom::PluginStatus::kAllowed;

  bool allow_wildcard = true;
  std::vector<WebPluginInfo> matching_plugins;
  std::vector<std::string> mime_types;
  PluginService::GetInstance()->GetPluginInfoArray(
      url, mime_type, allow_wildcard, &matching_plugins, &mime_types);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::erase_if(matching_plugins, [&](const WebPluginInfo& info) {
    return info.path.value() == ChromeContentClient::kNotPresent;
  });
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (matching_plugins.empty()) {
    *status = chrome::mojom::PluginStatus::kNotFound;
    return false;
  }

  content::PluginServiceFilter* filter =
      PluginService::GetInstance()->GetFilter();
  content::RenderProcessHost* rph =
      content::RenderProcessHost::FromID(render_process_id_);
  content::BrowserContext* browser_context =
      rph ? rph->GetBrowserContext() : nullptr;
  size_t i = 0;
  for (; i < matching_plugins.size(); ++i) {
    if (!filter ||
        filter->IsPluginAvailable(browser_context, matching_plugins[i])) {
      break;
    }
  }

  // If we broke out of the loop, we have found an enabled plugin.
  bool enabled = i < matching_plugins.size();
  if (!enabled) {
    // Otherwise, we only found disabled plugins, so we take the first one.
    i = 0;
    *status = chrome::mojom::PluginStatus::kDisabled;
  }

  *plugin = matching_plugins[i];
  *actual_mime_type = mime_types[i];
  if (plugin_metadata)
    *plugin_metadata = GetPluginMetadata(*plugin);

  return enabled;
}

void PluginInfoHostImpl::GetPluginInfoFinish(
    const GetPluginInfo_Params& params,
    chrome::mojom::PluginInfoPtr output,
    GetPluginInfoCallback callback,
    std::unique_ptr<PluginMetadata> plugin_metadata) {
  if (plugin_metadata) {
    output->group_identifier = plugin_metadata->identifier();
    output->group_name = plugin_metadata->name();
  }

  context_.MaybeGrantAccess(output->status, output->plugin.path);

  std::move(callback).Run(std::move(output));
}

// static
void PluginInfoHostImpl::EnsureFactoryBuilt() {
  PluginInfoHostImplShutdownNotifierFactory::GetInstance();
}

void PluginInfoHostImpl::Context::MaybeGrantAccess(
    chrome::mojom::PluginStatus status,
    const base::FilePath& path) const {
  if (status == chrome::mojom::PluginStatus::kAllowed ||
      status == chrome::mojom::PluginStatus::kPlayImportantContent) {
    ChromePluginServiceFilter::GetInstance()->AuthorizePlugin(
        render_process_id_, path);
  }
}

bool PluginInfoHostImpl::Context::IsPluginEnabled(
    const content::WebPluginInfo& plugin) const {
  return plugin_prefs_->IsPluginEnabled(plugin);
}
