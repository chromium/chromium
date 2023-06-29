// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_CHROME_PLUGIN_SERVICE_FILTER_H_
#define CHROME_BROWSER_PLUGINS_CHROME_PLUGIN_SERVICE_FILTER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/browser/render_process_host_observer.h"

class Profile;

namespace content {
class WebContents;
}

// This class must be created (by calling the |GetInstance| method) on the UI
// thread, but is safe to use on any thread after that.
class ChromePluginServiceFilter : public content::PluginServiceFilter,
                                  public content::RenderProcessHostObserver {
 public:
  static ChromePluginServiceFilter* GetInstance();

  // This method should be called on the UI thread.
  void RegisterProfile(Profile* profile);

  void UnregisterProfile(Profile* profile);

  // Authorizes a given plugin for a given process.
  void AuthorizePlugin(int render_process_id,
                       const base::FilePath& plugin_path);

  // Authorizes all plugins for a given WebContents. If |load_blocked| is true,
  // then the renderer is told to load the plugin with given |identifier| (or
  // pllugins if |identifier| is empty).
  // This method can only be called on the UI thread.
  void AuthorizeAllPlugins(content::WebContents* web_contents,
                           bool load_blocked,
                           const std::string& identifier);

  // PluginServiceFilter implementation.
  bool IsPluginAvailable(content::BrowserContext* browser_context,
                         const content::WebPluginInfo& plugin) override;

  // CanLoadPlugin always grants permission to the browser
  // (render_process_id == 0)
  bool CanLoadPlugin(int render_process_id,
                     const base::FilePath& path) override;

  // RenderProcessHostObserver implementation.
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;

  // Sets a callback to be called when `this` adds itself as an observer to a
  // RenderProcessHost.
  void NotifyIfObserverAddedForTesting(
      base::RepeatingClosure observer_added_callback_for_testing);

 private:
  friend struct base::DefaultSingletonTraits<ChromePluginServiceFilter>;
  struct ContextInfo;

  struct ProcessDetails {
    ProcessDetails();
    ProcessDetails(const ProcessDetails& other);
    ~ProcessDetails();

    std::set<base::FilePath> authorized_plugins;
  };

  ChromePluginServiceFilter();
  ~ChromePluginServiceFilter() override;

  ProcessDetails* GetOrRegisterProcess(int render_process_id);
  const ProcessDetails* GetProcess(int render_process_id) const;
  void ObserveRenderProcessHost(int render_process_id);

  base::Lock lock_;  // Guards access to member variables.

  std::map<const void*, std::unique_ptr<ContextInfo>> browser_context_map_;

  std::map<int, ProcessDetails> plugin_details_;

  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      host_observation_{this};

  base::RepeatingClosure observer_added_callback_for_testing_;

  base::WeakPtrFactory<ChromePluginServiceFilter> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PLUGINS_CHROME_PLUGIN_SERVICE_FILTER_H_
