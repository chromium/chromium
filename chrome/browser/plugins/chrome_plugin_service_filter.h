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
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/plugin_service_filter.h"

class Profile;

namespace content {
class WebContents;
}

// This class must be created (by calling the |GetInstance| method) on the UI
// thread, but is safe to use on any thread after that.
class ChromePluginServiceFilter : public content::PluginServiceFilter,
                                  public content::NotificationObserver {
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

  // content::NotificationObserver implementation:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  ProcessDetails* GetOrRegisterProcess(int render_process_id);
  const ProcessDetails* GetProcess(int render_process_id) const;

  content::NotificationRegistrar registrar_;

  base::Lock lock_;  // Guards access to member variables.

  std::map<const void*, std::unique_ptr<ContextInfo>> browser_context_map_;

  std::map<int, ProcessDetails> plugin_details_;
};

#endif  // CHROME_BROWSER_PLUGINS_CHROME_PLUGIN_SERVICE_FILTER_H_
