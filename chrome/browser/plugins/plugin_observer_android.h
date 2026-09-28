// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_ANDROID_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_ANDROID_H_

#include "base/memory/raw_ref.h"
#include "chrome/common/plugin.mojom.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace content {
class WebContents;
}

namespace tabs {
class TabInterface;
}

// Simplified version of PluginObserver used on Android. Note that this is built
// even though plugins are not enabled on Android.
class PluginObserverAndroid : public chrome::mojom::PluginHost {
 public:
  DECLARE_USER_DATA(PluginObserverAndroid);

  static void BindPluginHost(
      mojo::PendingAssociatedReceiver<chrome::mojom::PluginHost> receiver,
      content::RenderFrameHost* rfh);

  static PluginObserverAndroid* From(tabs::TabInterface* tab);

  PluginObserverAndroid(tabs::TabInterface& tab,
                        content::WebContents* web_contents);

  PluginObserverAndroid(const PluginObserverAndroid&) = delete;
  PluginObserverAndroid& operator=(const PluginObserverAndroid&) = delete;

  ~PluginObserverAndroid() override;

 private:
  // chrome::mojom::PluginHost:
  void OpenPDF(const GURL& url) override;

  const raw_ref<content::WebContents> web_contents_;
  content::RenderFrameHostReceiverSet<chrome::mojom::PluginHost>
      plugin_host_receivers_;
  ui::ScopedUnownedUserData<PluginObserverAndroid> scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_ANDROID_H_
