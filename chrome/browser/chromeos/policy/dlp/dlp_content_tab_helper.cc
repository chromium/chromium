// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_tab_helper.h"

#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace policy {

DlpContentTabHelper::~DlpContentTabHelper() = default;

void DlpContentTabHelper::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  const DlpContentRestrictionSet restriction_set =
      DlpContentManager::Get()->GetRestrictionSetForURL(
          render_frame_host->GetLastCommittedURL());
  if (!restriction_set.IsEmpty())
    AddFrame(render_frame_host, restriction_set);
}

void DlpContentTabHelper::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  RemoveFrame(render_frame_host);
}

void DlpContentTabHelper::RenderFrameHostStateChanged(
    content::RenderFrameHost* render_frame_host,
    content::RenderFrameHost::LifecycleState old_state,
    content::RenderFrameHost::LifecycleState new_state) {
  const DlpContentRestrictionSet restriction_set =
      DlpContentManager::Get()->GetRestrictionSetForURL(
          render_frame_host->GetLastCommittedURL());

  using LifecycleState = content::RenderFrameHost::LifecycleState;
  if (old_state != LifecycleState::kActive &&
      new_state == LifecycleState::kActive) {
    if (!restriction_set.IsEmpty())
      AddFrame(render_frame_host, restriction_set);
  } else if (old_state == LifecycleState::kActive &&
             new_state != LifecycleState::kActive) {
    RemoveFrame(render_frame_host);
  }
}

void DlpContentTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() || navigation_handle->IsErrorPage())
    return;
  const DlpContentRestrictionSet restriction_set =
      DlpContentManager::Get()->GetRestrictionSetForURL(
          navigation_handle->GetURL());
  if (restriction_set.IsEmpty()) {
    RemoveFrame(navigation_handle->GetRenderFrameHost());
  } else {
    AddFrame(navigation_handle->GetRenderFrameHost(), restriction_set);
  }
}

void DlpContentTabHelper::WebContentsDestroyed() {
  DlpContentManager::Get()->OnWebContentsDestroyed(web_contents());
}

void DlpContentTabHelper::OnVisibilityChanged(content::Visibility visibility) {
  // DlpContentManager tracks visibility only for confidential WebContents.
  if (GetRestrictionSet().IsEmpty())
    return;
  DlpContentManager::Get()->OnVisibilityChanged(web_contents());
}

DlpContentTabHelper::DlpContentTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

DlpContentRestrictionSet DlpContentTabHelper::GetRestrictionSet() const {
  DlpContentRestrictionSet set;
  for (auto& entry : confidential_frames_) {
    set.UnionWith(entry.second);
  }
  return set;
}

void DlpContentTabHelper::AddFrame(content::RenderFrameHost* render_frame_host,
                                   DlpContentRestrictionSet restrictions) {
  const DlpContentRestrictionSet old_restriction_set = GetRestrictionSet();
  confidential_frames_[render_frame_host] = restrictions;
  const DlpContentRestrictionSet new_restriction_set = GetRestrictionSet();
  if (new_restriction_set != old_restriction_set) {
    DlpContentManager::Get()->OnConfidentialityChanged(web_contents(),
                                                       new_restriction_set);
  }
}

void DlpContentTabHelper::RemoveFrame(
    content::RenderFrameHost* render_frame_host) {
  const DlpContentRestrictionSet old_restriction_set = GetRestrictionSet();
  confidential_frames_.erase(render_frame_host);
  const DlpContentRestrictionSet new_restriction_set = GetRestrictionSet();
  if (old_restriction_set != new_restriction_set) {
    DlpContentManager::Get()->OnConfidentialityChanged(web_contents(),
                                                       new_restriction_set);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DlpContentTabHelper)

}  // namespace policy
