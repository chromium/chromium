// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_observer_android.h"

#include <utility>

#include "chrome/browser/plugins/plugin_observer_common.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"

void PluginObserverAndroid::BindPluginHost(
    mojo::PendingAssociatedReceiver<chrome::mojom::PluginHost> receiver,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* plugin_helper = PluginObserverAndroid::FromWebContents(web_contents);
  if (!plugin_helper)
    return;
  plugin_helper->plugin_host_receivers_.Bind(rfh, std::move(receiver));
}

PluginObserverAndroid::PluginObserverAndroid(content::WebContents* web_contents)
    : content::WebContentsUserData<PluginObserverAndroid>(*web_contents),
      plugin_host_receivers_(web_contents, this) {}

PluginObserverAndroid::~PluginObserverAndroid() = default;

void PluginObserverAndroid::OpenPDF(const GURL& url) {
  content::RenderFrameHost* render_frame_host =
      plugin_host_receivers_.GetCurrentTargetFrame();

  content::Referrer referrer;
  if (!CanOpenPdfUrl(render_frame_host, url,
                     GetWebContents().GetLastCommittedURL(), &referrer)) {
    return;
  }

  content::OpenURLParams open_url_params(
      url, referrer, WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
  // On Android, PDFs downloaded with a user gesture are auto-opened.
  open_url_params.user_gesture = true;
  GetWebContents().OpenURL(open_url_params, /*navigation_handle_callback=*/{});
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PluginObserverAndroid);
