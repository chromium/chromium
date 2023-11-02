// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/plugin.mojom.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ppapi/buildflags/buildflags.h"

#if !BUILDFLAG(ENABLE_PLUGINS)
#error "Plugins should be enabled"
#endif

namespace infobars {
class ContentInfoBarManager;
}

namespace content {
class WebContents;
}

class PluginObserver : public content::WebContentsObserver,
                       public chrome::mojom::PluginHost,
                       public content::WebContentsUserData<PluginObserver> {
 public:
  static void BindPluginHost(
      mojo::PendingAssociatedReceiver<chrome::mojom::PluginHost> receiver,
      content::RenderFrameHost* rfh);

  PluginObserver(const PluginObserver&) = delete;
  PluginObserver& operator=(const PluginObserver&) = delete;

  ~PluginObserver() override;

  // content::WebContentsObserver implementation.
  void PluginCrashed(const base::FilePath& plugin_path,
                     base::ProcessId plugin_pid) override;

  // Public for tests only.
  static void CreatePluginObserverInfoBar(
      infobars::ContentInfoBarManager* infobar_manager,
      const std::u16string& plugin_name);

 private:
  friend class content::WebContentsUserData<PluginObserver>;

  explicit PluginObserver(content::WebContents* web_contents);

  // chrome::mojom::PluginHost methods.
  void CouldNotLoadPlugin(const base::FilePath& plugin_path) override;
  void OpenPDF(const GURL& url) override;

  content::RenderFrameHostReceiverSet<chrome::mojom::PluginHost>
      plugin_host_receivers_;

  base::WeakPtrFactory<PluginObserver> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_H_
