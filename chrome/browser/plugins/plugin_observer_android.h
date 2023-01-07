// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_ANDROID_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_ANDROID_H_

#include "chrome/common/plugin.mojom.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {
class WebContents;
}

// Simplified version of PluginObserver used on Android. Note that this is built
// even though plugins are not enabled on Android.
class PluginObserverAndroid
    : public chrome::mojom::PluginHost,
      public content::WebContentsUserData<PluginObserverAndroid> {
 public:
  static void BindPluginHost(
      mojo::PendingAssociatedReceiver<chrome::mojom::PluginHost> receiver,
      content::RenderFrameHost* rfh);

  PluginObserverAndroid(const PluginObserverAndroid&) = delete;
  PluginObserverAndroid& operator=(const PluginObserverAndroid&) = delete;

  ~PluginObserverAndroid() override;

 private:
  friend class content::WebContentsUserData<PluginObserverAndroid>;

  explicit PluginObserverAndroid(content::WebContents* web_contents);

  // chrome::mojom::PluginHost:
  void OpenPDF(const GURL& url) override;

  content::RenderFrameHostReceiverSet<chrome::mojom::PluginHost>
      plugin_host_receivers_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_ANDROID_H_
