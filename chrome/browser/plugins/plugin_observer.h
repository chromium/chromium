// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/plugin.mojom.h"
#include "components/component_updater/component_updater_service.h"
#include "content/public/browser/web_contents_binding_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

class InfoBarService;

namespace content {
class WebContents;
}

class PluginObserver : public content::WebContentsObserver,
                       public chrome::mojom::PluginHost,
                       public content::WebContentsUserData<PluginObserver> {
 public:
  ~PluginObserver() override;

  // content::WebContentsObserver implementation.
  void PluginCrashed(const base::FilePath& plugin_path,
                     base::ProcessId plugin_pid) override;

  // Public for tests only.
  static void CreatePluginObserverInfoBar(InfoBarService* infobar_service,
                                          const base::string16& plugin_name);

 private:
  class ComponentObserver;
  class PluginPlaceholderHost;
  friend class content::WebContentsUserData<PluginObserver>;

  explicit PluginObserver(content::WebContents* web_contents);

  // chrome::mojom::PluginHost methods.
  void BlockedOutdatedPlugin(
      mojo::PendingRemote<chrome::mojom::PluginRenderer> plugin_renderer,
      const std::string& identifier) override;
  void BlockedComponentUpdatedPlugin(
      mojo::PendingRemote<chrome::mojom::PluginRenderer> plugin_renderer,
      const std::string& identifier) override;
  void ShowFlashPermissionBubble() override;
  void CouldNotLoadPlugin(const base::FilePath& plugin_path) override;

  void RemovePluginPlaceholderHost(PluginPlaceholderHost* placeholder);
  void RemoveComponentObserver(ComponentObserver* component_observer);

  // Stores all PluginPlaceholderHosts, keyed by memory address.
  std::map<PluginPlaceholderHost*, std::unique_ptr<PluginPlaceholderHost>>
      plugin_placeholders_;

  // Stores all ComponentObservers, keyed by memory address.
  std::map<ComponentObserver*, std::unique_ptr<ComponentObserver>>
      component_observers_;

  content::WebContentsFrameBindingSet<chrome::mojom::PluginHost>
      plugin_host_bindings_;

  base::WeakPtrFactory<PluginObserver> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(PluginObserver);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_H_
