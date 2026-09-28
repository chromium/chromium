// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_observer_android.h"

#include <utility>

#include "chrome/browser/plugins/plugin_observer_common.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

DEFINE_USER_DATA(PluginObserverAndroid);

void PluginObserverAndroid::BindPluginHost(
    mojo::PendingAssociatedReceiver<chrome::mojom::PluginHost> receiver,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* tab = tabs::TabInterface::MaybeGetFromContents(web_contents);
  auto* plugin_helper = tab ? PluginObserverAndroid::From(tab) : nullptr;
  if (!plugin_helper)
    return;
  plugin_helper->plugin_host_receivers_.Bind(rfh, std::move(receiver));
}

// static
PluginObserverAndroid* PluginObserverAndroid::From(tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

PluginObserverAndroid::PluginObserverAndroid(tabs::TabInterface& tab,
                                             content::WebContents* web_contents)
    : web_contents_(*web_contents),
      plugin_host_receivers_(web_contents, this),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {}

PluginObserverAndroid::~PluginObserverAndroid() = default;

void PluginObserverAndroid::OpenPDF(const GURL& url) {
  content::RenderFrameHost& render_frame_host =
      plugin_host_receivers_.CurrentTargetFrame();

  content::Referrer referrer;
  if (!CanOpenPdfUrl(render_frame_host, url,
                     web_contents_->GetLastCommittedURL(), &referrer)) {
    return;
  }

  content::OpenURLParams open_url_params =
      content::OpenURLParams::CreateBrowserInitiated(
          url, WindowOpenDisposition::CURRENT_TAB,
          ui::PAGE_TRANSITION_AUTO_BOOKMARK, referrer);
  // On Android, PDFs downloaded with a user gesture are auto-opened.
  open_url_params.user_gesture = true;
  web_contents_->OpenURL(open_url_params, /*navigation_handle_callback=*/{});
}
