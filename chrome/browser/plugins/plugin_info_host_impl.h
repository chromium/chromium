// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_INFO_HOST_IMPL_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_INFO_HOST_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/common/plugin.mojom.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "components/prefs/pref_member.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

class GURL;
class HostContentSettingsMap;
class Profile;

namespace component_updater {
struct ComponentInfo;
}

namespace content {
struct WebPluginInfo;
}  // namespace content

namespace extensions {
class ExtensionRegistry;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace url {
class Origin;
}

// Implements PluginInfoHost interface.
class PluginInfoHostImpl : public chrome::mojom::PluginInfoHost {
 public:
  struct GetPluginInfo_Params;

  // Contains all the information needed by the PluginInfoHostImpl.
  class Context {
   public:
    Context(int render_process_id, Profile* profile);

    ~Context();

    int render_process_id() { return render_process_id_; }

    void DecidePluginStatus(const GURL& url,
                            const url::Origin& main_frame_origin,
                            const content::WebPluginInfo& plugin,
                            PluginMetadata::SecurityStatus security_status,
                            const std::string& plugin_identifier,
                            chrome::mojom::PluginStatus* status) const;
    bool FindEnabledPlugin(
        int render_frame_id,
        const GURL& url,
        const url::Origin& main_frame_origin,
        const std::string& mime_type,
        chrome::mojom::PluginStatus* status,
        content::WebPluginInfo* plugin,
        std::string* actual_mime_type,
        std::unique_ptr<PluginMetadata>* plugin_metadata) const;
    void MaybeGrantAccess(chrome::mojom::PluginStatus status,
                          const base::FilePath& path) const;
    bool IsPluginEnabled(const content::WebPluginInfo& plugin) const;

    void ShutdownOnUIThread();

   private:
    int render_process_id_;
#if BUILDFLAG(ENABLE_EXTENSIONS)
    extensions::ExtensionRegistry* extension_registry_;
#endif
    const HostContentSettingsMap* host_content_settings_map_;
    scoped_refptr<PluginPrefs> plugin_prefs_;

    BooleanPrefMember allow_outdated_plugins_;
    BooleanPrefMember run_all_flash_in_allow_mode_;
  };

  PluginInfoHostImpl(int render_process_id, Profile* profile);
  ~PluginInfoHostImpl() override;

  static void RegisterUserPrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  void ShutdownOnUIThread();

  // chrome::mojom::PluginInfoHost
  void GetPluginInfo(int32_t render_frame_id,
                     const GURL& url,
                     const url::Origin& origin,
                     const std::string& mime_type,
                     GetPluginInfoCallback callback) override;

  // |params| wraps the parameters passed to |OnGetPluginInfo|, because
  // |base::Bind| doesn't support the required arity <http://crbug.com/98542>.
  void PluginsLoaded(const GetPluginInfo_Params& params,
                     GetPluginInfoCallback callback,
                     const std::vector<content::WebPluginInfo>& plugins);

  void ComponentPluginLookupDone(
      const GetPluginInfo_Params& params,
      chrome::mojom::PluginInfoPtr output,
      GetPluginInfoCallback callback,
      std::unique_ptr<PluginMetadata> plugin_metadata,
      std::unique_ptr<component_updater::ComponentInfo> cus_plugin_info);

  void GetPluginInfoFinish(const GetPluginInfo_Params& params,
                           chrome::mojom::PluginInfoPtr output,
                           GetPluginInfoCallback callback,
                           std::unique_ptr<PluginMetadata> plugin_metadata);

  // Reports usage metrics to UKM.
  void ReportMetrics(int render_frame_id,
                     const base::StringPiece& mime_type,
                     const url::Origin& main_frame_origin);

  Context context_;
  std::unique_ptr<KeyedServiceShutdownNotifier::Subscription>
      shutdown_notifier_;

  base::WeakPtrFactory<PluginInfoHostImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PluginInfoHostImpl);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_INFO_HOST_IMPL_H_
