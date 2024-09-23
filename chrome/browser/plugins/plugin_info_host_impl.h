// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_INFO_HOST_IMPL_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_INFO_HOST_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/common/plugin.mojom.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"

class GURL;
class HostContentSettingsMap;
class Profile;

namespace content {
struct WebPluginInfo;
}  // namespace content

namespace extensions {
class ExtensionRegistry;
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
        const GURL& url,
        const std::string& mime_type,
        chrome::mojom::PluginStatus* status,
        content::WebPluginInfo* plugin,
        std::string* actual_mime_type,
        std::unique_ptr<PluginMetadata>* plugin_metadata) const;
    void MaybeGrantAccess(chrome::mojom::PluginStatus status,
                          const base::FilePath& path) const;
    bool IsPluginEnabled(const content::WebPluginInfo& plugin) const;

   private:
    int render_process_id_;
#if BUILDFLAG(ENABLE_EXTENSIONS)
    raw_ptr<extensions::ExtensionRegistry, DanglingUntriaged>
        extension_registry_;
#endif
    raw_ptr<const HostContentSettingsMap, AcrossTasksDanglingUntriaged>
        host_content_settings_map_;
    scoped_refptr<PluginPrefs> plugin_prefs_;
  };

  PluginInfoHostImpl(int render_process_id, Profile* profile);

  PluginInfoHostImpl(const PluginInfoHostImpl&) = delete;
  PluginInfoHostImpl& operator=(const PluginInfoHostImpl&) = delete;

  ~PluginInfoHostImpl() override;

  // chrome::mojom::PluginInfoHost
  void GetPluginInfo(const GURL& url,
                     const url::Origin& origin,
                     const std::string& mime_type,
                     GetPluginInfoCallback callback) override;

  static void EnsureFactoryBuilt();

 private:
  void ShutdownOnUIThread();

  // |params| wraps the parameters passed to |OnGetPluginInfo|, because
  // |base::Bind| doesn't support the required arity <http://crbug.com/98542>.
  void PluginsLoaded(const GetPluginInfo_Params& params,
                     GetPluginInfoCallback callback,
                     const std::vector<content::WebPluginInfo>& plugins);

  void GetPluginInfoFinish(const GetPluginInfo_Params& params,
                           chrome::mojom::PluginInfoPtr output,
                           GetPluginInfoCallback callback,
                           std::unique_ptr<PluginMetadata> plugin_metadata);

  Context context_;
  base::CallbackListSubscription shutdown_subscription_;

  base::WeakPtrFactory<PluginInfoHostImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_INFO_HOST_IMPL_H_
