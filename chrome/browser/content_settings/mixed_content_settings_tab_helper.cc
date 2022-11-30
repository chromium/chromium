// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/mixed_content_settings_tab_helper.h"

#include "base/containers/contains.h"
#include "components/content_settings/common/content_settings_agent.mojom.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

using content::BrowserThread;
using content::RenderFrameHost;
using content::WebContents;

MixedContentSettingsTabHelper::MixedContentSettingsTabHelper(WebContents* tab)
    : content::WebContentsObserver(tab),
      content::WebContentsUserData<MixedContentSettingsTabHelper>(*tab) {
  if (!tab->HasOpener())
    return;

  // Note: using the opener WebContents to override these values only works
  // because in Chrome these settings are maintained at the tab level instead of
  // at the frame level as Blink does.
  MixedContentSettingsTabHelper* opener_settings =
      MixedContentSettingsTabHelper::FromWebContents(
          WebContents::FromRenderFrameHost(tab->GetOpener()));
  if (opener_settings &&
      opener_settings->IsRunningInsecureContentAllowed(*tab->GetOpener())) {
    AllowRunningOfInsecureContent(*tab->GetPrimaryMainFrame());
  }
}

MixedContentSettingsTabHelper::~MixedContentSettingsTabHelper() {}

void MixedContentSettingsTabHelper::AllowRunningOfInsecureContent(
    RenderFrameHost& render_frame_host) {
  DCHECK(!render_frame_host.IsNestedWithinFencedFrame());
  auto* main_frame = render_frame_host.GetOutermostMainFrame();
  if (!base::Contains(settings_, main_frame)) {
    settings_[main_frame] = std::make_unique<PageSettings>(main_frame);
  }
  settings_[main_frame]->AllowRunningOfInsecureContent();
}

void MixedContentSettingsTabHelper::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (!IsRunningInsecureContentAllowed(*render_frame_host))
    return;

  // Fenced Frames should never allow insecure content.
  DCHECK(!render_frame_host->IsNestedWithinFencedFrame());
  mojo::AssociatedRemote<content_settings::mojom::ContentSettingsAgent> agent;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&agent);
  agent->SetAllowRunningInsecureContent();
}

void MixedContentSettingsTabHelper::RenderFrameDeleted(RenderFrameHost* frame) {
  settings_.erase(frame);
}

bool MixedContentSettingsTabHelper::IsRunningInsecureContentAllowed(
    RenderFrameHost& render_frame_host) {
  // If render_frame_host is not nested in a Fenced Frame then the
  // InsecureContent of the outermost main frame applies. If render_frame_host
  // is a frame that is the root of a Fenced Frame or is nested inside a Fenced
  // Frame the Insecure Content setting is ignored.
  if (render_frame_host.IsNestedWithinFencedFrame())
    return false;
  auto setting_it = settings_.find(render_frame_host.GetOutermostMainFrame());
  if (setting_it == settings_.end())
    return false;
  return setting_it->second->is_running_insecure_content_allowed();
}

MixedContentSettingsTabHelper::PageSettings::PageSettings(
    RenderFrameHost* main_frame_host) {
  DCHECK(!main_frame_host->GetParent());
}

void MixedContentSettingsTabHelper::PageSettings::
    AllowRunningOfInsecureContent() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  is_running_insecure_content_allowed_ = true;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MixedContentSettingsTabHelper);
