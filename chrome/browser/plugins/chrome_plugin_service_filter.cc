// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/chrome_plugin_service_filter.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

using content::BrowserThread;
using content::PluginService;

// ChromePluginServiceFilter inner struct definitions.

struct ChromePluginServiceFilter::ContextInfo {
  ContextInfo(scoped_refptr<PluginPrefs> pp,
              scoped_refptr<HostContentSettingsMap> hcsm,
              Profile* profile);

  ContextInfo(const ContextInfo&) = delete;
  ContextInfo& operator=(const ContextInfo&) = delete;

  ~ContextInfo();

  scoped_refptr<PluginPrefs> plugin_prefs;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map;
};

ChromePluginServiceFilter::ContextInfo::ContextInfo(
    scoped_refptr<PluginPrefs> pp,
    scoped_refptr<HostContentSettingsMap> hcsm,
    Profile* profile)
    : plugin_prefs(std::move(pp)), host_content_settings_map(std::move(hcsm)) {}

ChromePluginServiceFilter::ContextInfo::~ContextInfo() = default;

ChromePluginServiceFilter::ProcessDetails::ProcessDetails() = default;

ChromePluginServiceFilter::ProcessDetails::ProcessDetails(
    const ProcessDetails& other) = default;

ChromePluginServiceFilter::ProcessDetails::~ProcessDetails() = default;

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
      HostContentSettingsMapFactory::GetForProfile(profile), profile);
}

void ChromePluginServiceFilter::UnregisterProfile(Profile* profile) {
  base::AutoLock lock(lock_);
  browser_context_map_.erase(profile);
}

void ChromePluginServiceFilter::AuthorizePlugin(
    int render_process_id,
    const base::FilePath& plugin_path) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ChromePluginServiceFilter::ObserveRenderProcessHost,
                     weak_factory_.GetWeakPtr(), render_process_id));
  base::AutoLock auto_lock(lock_);
  ProcessDetails* details = GetOrRegisterProcess(render_process_id);
  details->authorized_plugins.insert(plugin_path);
}

void ChromePluginServiceFilter::AuthorizeAllPlugins(
    content::WebContents* web_contents,
    bool load_blocked,
    const std::string& identifier) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Authorize all plugins is intended for the granting access to only
  // the currently active page, so we iterate on the main frame.
  web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [](content::RenderFrameHost* render_frame_host) {
        ChromePluginServiceFilter::GetInstance()->AuthorizePlugin(
            render_frame_host->GetProcess()->GetID(), base::FilePath());
      });

  if (load_blocked) {
    web_contents->GetPrimaryMainFrame()
        ->ForEachRenderFrameHost(
            [&identifier](content::RenderFrameHost* render_frame_host) {
              mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
                  chrome_render_frame;
              render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
                  &chrome_render_frame);
              chrome_render_frame->LoadBlockedPlugins(identifier);
            });
  }
}

bool ChromePluginServiceFilter::IsPluginAvailable(
    content::BrowserContext* browser_context,
    const content::WebPluginInfo& plugin) {
  base::AutoLock auto_lock(lock_);

  // Check whether the plugin is disabled.
  auto context_info_it = browser_context_map_.find(browser_context);
  // The context might not be found because RenderFrameMessageFilter might
  // outlive the Profile (the context is unregistered during the Profile
  // destructor).
  if (context_info_it == browser_context_map_.end())
    return false;

  const ContextInfo* context_info = context_info_it->second.get();
  if (!context_info->plugin_prefs.get()->IsPluginEnabled(plugin))
    return false;

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

void ChromePluginServiceFilter::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::AutoLock auto_lock(lock_);
  plugin_details_.erase(host->GetID());
  host_observation_.RemoveObservation(host);
}

void ChromePluginServiceFilter::NotifyIfObserverAddedForTesting(
    base::RepeatingClosure observer_added_callback_for_testing) {
  observer_added_callback_for_testing_ =
      std::move(observer_added_callback_for_testing);
}

ChromePluginServiceFilter::ChromePluginServiceFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ChromePluginServiceFilter::~ChromePluginServiceFilter() = default;

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
    return nullptr;
  return &it->second;
}

void ChromePluginServiceFilter::ObserveRenderProcessHost(
    int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id);
  if (!host) {
    // The RenderProcessHost ceased to exist before we could observe it.
    base::AutoLock auto_lock(lock_);
    plugin_details_.erase(render_process_id);
    return;
  }
  if (!host_observation_.IsObservingSource(host)) {
    host_observation_.AddObservation(host);
    if (observer_added_callback_for_testing_) {
      observer_added_callback_for_testing_.Run();
    }
  }
}
