// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/plugin.mojom.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/buildflags.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

#if !BUILDFLAG(ENABLE_PLUGINS)
#error "Plugins should be enabled"
#endif

namespace content {
class WebContents;
}

namespace tabs {
class TabInterface;
}

class PluginObserver : public content::WebContentsObserver,
                       public chrome::mojom::PluginHost {
 public:
  DECLARE_USER_DATA(PluginObserver);

  static void BindPluginHost(
      mojo::PendingAssociatedReceiver<chrome::mojom::PluginHost> receiver,
      content::RenderFrameHost* rfh);

  static PluginObserver* From(tabs::TabInterface* tab);

  PluginObserver(tabs::TabInterface& tab, content::WebContents* web_contents);

  PluginObserver(const PluginObserver&) = delete;
  PluginObserver& operator=(const PluginObserver&) = delete;

  ~PluginObserver() override;

 private:
  // chrome::mojom::PluginHost methods.
  void OpenPDF(const GURL& url) override;

  content::RenderFrameHostReceiverSet<chrome::mojom::PluginHost>
      plugin_host_receivers_;

  ui::ScopedUnownedUserData<PluginObserver> scoped_unowned_user_data_;

  base::WeakPtrFactory<PluginObserver> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_H_
