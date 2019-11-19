// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/chrome_plugin_service_filter.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/plugins/flash_temporary_permission_tracker.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/render_messages.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"

using content::BrowserThread;
using content::PluginService;

namespace {

class ProfileContentSettingObserver : public content_settings::Observer {
 public:
  explicit ProfileContentSettingObserver(Profile* profile)
      : profile_(profile) {}
  ~ProfileContentSettingObserver() override {}
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const std::string& resource_identifier) override {
    if (content_type != ContentSettingsType::PLUGINS)
      return;

    // We must purge the plugin list cache when the plugin content setting
    // changes, because the content setting affects the visibility of Flash.
    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(profile_);
    PluginService::GetInstance()->PurgePluginListCache(profile_, false);

    const GURL primary(primary_pattern.ToString());
    if (primary.is_valid()) {
      DCHECK_EQ(ContentSettingsPattern::Relation::IDENTITY,
                ContentSettingsPattern::Wildcard().Compare(secondary_pattern));
      PluginUtils::RememberFlashChangedForSite(map, primary);
    }
  }

 private:
  Profile* profile_;
};

void AuthorizeRenderer(content::RenderFrameHost* render_frame_host) {
  ChromePluginServiceFilter::GetInstance()->AuthorizePlugin(
      render_frame_host->GetProcess()->GetID(), base::FilePath());
}

}  // namespace

// ChromePluginServiceFilter inner struct definitions.

struct ChromePluginServiceFilter::ContextInfo {
  ContextInfo(scoped_refptr<PluginPrefs> pp,
              scoped_refptr<HostContentSettingsMap> hcsm,
              scoped_refptr<FlashTemporaryPermissionTracker> ftpm,
              Profile* profile);
  ~ContextInfo();

  scoped_refptr<PluginPrefs> plugin_prefs;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map;
  scoped_refptr<FlashTemporaryPermissionTracker> permission_tracker;
  ProfileContentSettingObserver observer;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContextInfo);
};

ChromePluginServiceFilter::ContextInfo::ContextInfo(
    scoped_refptr<PluginPrefs> pp,
    scoped_refptr<HostContentSettingsMap> hcsm,
    scoped_refptr<FlashTemporaryPermissionTracker> ftpm,
    Profile* profile)
    : plugin_prefs(std::move(pp)),
      host_content_settings_map(std::move(hcsm)),
      permission_tracker(std::move(ftpm)),
      observer(profile) {
  host_content_settings_map->AddObserver(&observer);
}

ChromePluginServiceFilter::ContextInfo::~ContextInfo() {
  host_content_settings_map->RemoveObserver(&observer);
}

ChromePluginServiceFilter::ProcessDetails::ProcessDetails() {}

ChromePluginServiceFilter::ProcessDetails::ProcessDetails(
    const ProcessDetails& other) = default;

ChromePluginServiceFilter::ProcessDetails::~ProcessDetails() {}

// ChromePluginServiceFilter definitions.

// static
ChromePluginServiceFilter* ChromePluginServiceFilter::GetInstance() {
  return base::Singleton<ChromePluginServiceFilter>::get();
}

void ChromePluginServiceFilter::RegisterProfile(Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::AutoLock lock(lock_);
  browser_context_map_[profile] = std::make_unique<ContextInfo>(
      PluginPrefs::GetForProfile(profile),
      HostContentSettingsMapFactory::GetForProfile(profile),
      FlashTemporaryPermissionTracker::Get(profile), profile);
}

void ChromePluginServiceFilter::UnregisterProfile(Profile* profile) {
  base::AutoLock lock(lock_);
  browser_context_map_.erase(profile);
}

void ChromePluginServiceFilter::OverridePluginForFrame(
    int render_process_id,
    int render_frame_id,
    const content::WebPluginInfo& plugin) {
  base::AutoLock auto_lock(lock_);
  ProcessDetails* details = GetOrRegisterProcess(render_process_id);
  details->overridden_plugins.push_back({render_frame_id, plugin});
}

void ChromePluginServiceFilter::AuthorizePlugin(
    int render_process_id,
    const base::FilePath& plugin_path) {
  base::AutoLock auto_lock(lock_);
  ProcessDetails* details = GetOrRegisterProcess(render_process_id);
  details->authorized_plugins.insert(plugin_path);
}

void ChromePluginServiceFilter::AuthorizeAllPlugins(
    content::WebContents* web_contents,
    bool load_blocked,
    const std::string& identifier) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  web_contents->ForEachFrame(base::BindRepeating(&AuthorizeRenderer));
  if (load_blocked) {
    web_contents->SendToAllFrames(new ChromeViewMsg_LoadBlockedPlugins(
        MSG_ROUTING_NONE, identifier));
  }
}

bool ChromePluginServiceFilter::IsPluginAvailable(
    int render_process_id,
    int render_frame_id,
    const GURL& plugin_content_url,
    const url::Origin& main_frame_origin,
    content::WebPluginInfo* plugin) {
  base::AutoLock auto_lock(lock_);
  const ProcessDetails* details = GetProcess(render_process_id);

  // Check whether the plugin is overridden.
  if (details) {
    for (const auto& plugin_override : details->overridden_plugins) {
      if (plugin_override.render_frame_id == render_frame_id) {
        bool use = plugin_override.plugin.path == plugin->path;
        if (use)
          *plugin = plugin_override.plugin;
        return use;
      }
    }
  }

  content::RenderProcessHost* rph =
      content::RenderProcessHost::FromID(render_process_id);
  if (!rph)
    return false;

  // Check whether the plugin is disabled.
  auto context_info_it = browser_context_map_.find(rph->GetBrowserContext());
  // The context might not be found because RenderFrameMessageFilter might
  // outlive the Profile (the context is unregistered during the Profile
  // destructor).
  if (context_info_it == browser_context_map_.end())
    return false;

  const ContextInfo* context_info = context_info_it->second.get();
  if (!context_info->plugin_prefs.get()->IsPluginEnabled(*plugin))
    return false;

  // Do additional checks for Flash.
  if (plugin->name == base::ASCIIToUTF16(content::kFlashPluginName)) {
    // Check the content setting first, and always respect the ALLOW or BLOCK
    // state. When IsPluginAvailable() is called to check whether a plugin
    // should be advertised, |url| has the same origin as |main_frame_origin|.
    // The intended behavior is that Flash is advertised only if a Flash embed
    // hosted on the same origin as the main frame origin is allowed to run.
    bool is_managed = false;
    HostContentSettingsMap* settings_map =
        context_info_it->second->host_content_settings_map.get();
    ContentSetting flash_setting = PluginUtils::GetFlashPluginContentSetting(
        settings_map, main_frame_origin, plugin_content_url, &is_managed);

    if (flash_setting == CONTENT_SETTING_ALLOW)
      return true;

    if (flash_setting == CONTENT_SETTING_BLOCK)
      return false;

    // If the content setting is being managed by enterprise policy and is an
    // ASK setting, we check to see if it has been temporarily granted.
    if (is_managed) {
      return context_info_it->second->permission_tracker->IsFlashEnabled(
          main_frame_origin.GetURL());
    }

    return false;
  }

  return true;
}

bool ChromePluginServiceFilter::CanLoadPlugin(int render_process_id,
                                              const base::FilePath& path) {
  // The browser itself sometimes loads plugins to e.g. clear plugin data.
  // We always grant the browser permission.
  if (!render_process_id)
    return true;

  base::AutoLock auto_lock(lock_);
  const ProcessDetails* details = GetProcess(render_process_id);
  if (!details)
    return false;

  return (base::Contains(details->authorized_plugins, path) ||
          base::Contains(details->authorized_plugins, base::FilePath()));
}

ChromePluginServiceFilter::ChromePluginServiceFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllSources());
}

ChromePluginServiceFilter::~ChromePluginServiceFilter() {}

void ChromePluginServiceFilter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(content::NOTIFICATION_RENDERER_PROCESS_CLOSED, type);
  int render_process_id =
      content::Source<content::RenderProcessHost>(source).ptr()->GetID();

  base::AutoLock auto_lock(lock_);
  plugin_details_.erase(render_process_id);
}

ChromePluginServiceFilter::ProcessDetails*
ChromePluginServiceFilter::GetOrRegisterProcess(
    int render_process_id) {
  return &plugin_details_[render_process_id];
}

const ChromePluginServiceFilter::ProcessDetails*
ChromePluginServiceFilter::GetProcess(
    int render_process_id) const {
  auto it = plugin_details_.find(render_process_id);
  if (it == plugin_details_.end())
    return NULL;
  return &it->second;
}
