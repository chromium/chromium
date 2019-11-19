// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/mixed_content_settings_tab_helper.h"

#include "chrome/common/content_settings_agent.mojom.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

using content::BrowserThread;
using content::WebContents;

MixedContentSettingsTabHelper::MixedContentSettingsTabHelper(WebContents* tab)
    : content::WebContentsObserver(tab) {
  if (!tab->HasOpener())
    return;

  // Note: using the opener WebContents to override these values only works
  // because in Chrome these settings are maintained at the tab level instead of
  // at the frame level as Blink does.
  MixedContentSettingsTabHelper* opener_settings =
      MixedContentSettingsTabHelper::FromWebContents(
          WebContents::FromRenderFrameHost(tab->GetOpener()));
  if (opener_settings) {
    insecure_content_site_instance_ =
        opener_settings->insecure_content_site_instance_;
    is_running_insecure_content_allowed_ =
        opener_settings->is_running_insecure_content_allowed_;
  }
}

MixedContentSettingsTabHelper::~MixedContentSettingsTabHelper() {}

void MixedContentSettingsTabHelper::AllowRunningOfInsecureContent() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!insecure_content_site_instance_ ||
         insecure_content_site_instance_ == web_contents()->GetSiteInstance());
  insecure_content_site_instance_ = web_contents()->GetSiteInstance();
  is_running_insecure_content_allowed_ = true;
}

void MixedContentSettingsTabHelper::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (!is_running_insecure_content_allowed_)
    return;

  mojo::AssociatedRemote<chrome::mojom::ContentSettingsAgent> agent;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&agent);
  agent->SetAllowRunningInsecureContent();
}

void MixedContentSettingsTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
    return;

  // We will not be able to restore the state of these variables if we navigate
  // back and the page is in the BackForwardCache, so do not store it if we were
  // to lose that state.
  if (is_running_insecure_content_allowed_ || insecure_content_site_instance_) {
    content::BackForwardCache::DisableForRenderFrameHost(
        navigation_handle->GetPreviousRenderFrameHostId(),
        "MixedContentSettingsTabHelper");
  }

  // Resets mixed content settings on a successful navigation of the main frame
  // to a different SiteInstance. This follows the renderer side behavior which
  // is reset whenever a cross-site navigation takes place: a new main
  // RenderFrame is created along with a new ContentSettingsObserver, causing
  // the effective reset of the mixed content running permission.
  // Note: even though on the renderer this setting exists on a per frame basis,
  // it has always been controlled at the WebContents level. Its value is always
  // inherited from the parent frame and when changed the whole tree is updated.
  content::SiteInstance* new_site =
      navigation_handle->GetRenderFrameHost()->GetSiteInstance();
  if (new_site != insecure_content_site_instance_) {
    insecure_content_site_instance_ = nullptr;
    is_running_insecure_content_allowed_ = false;
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MixedContentSettingsTabHelper)
