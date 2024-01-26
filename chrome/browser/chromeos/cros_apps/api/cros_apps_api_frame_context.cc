// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_frame_context.h"

#include "base/functional/overloaded.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

CrosAppsApiFrameContext::CrosAppsApiFrameContext(content::RenderFrameHost& rfh)
    : context_(raw_ref(rfh)) {}

CrosAppsApiFrameContext::CrosAppsApiFrameContext(
    content::NavigationHandle& navigation_handle)
    : context_(raw_ref(navigation_handle)) {}

CrosAppsApiFrameContext::~CrosAppsApiFrameContext() = default;

const GURL& CrosAppsApiFrameContext::GetUrl() const {
  return absl::visit(
      base::Overloaded{
          [](const raw_ref<content::RenderFrameHost> rfh) -> const GURL& {
            return rfh->GetLastCommittedURL();
          },
          [](const raw_ref<content::NavigationHandle> navigation_handle)
              -> const GURL& { return navigation_handle->GetURL(); }},
      context_);
}

bool CrosAppsApiFrameContext::IsPrimaryMainFrame() const {
  return absl::visit(
      base::Overloaded{
          [](const raw_ref<content::RenderFrameHost> rfh) {
            return rfh->IsInPrimaryMainFrame();
          },
          [](const raw_ref<content::NavigationHandle> navigation_handle) {
            return navigation_handle->IsInPrimaryMainFrame();
          }},
      context_);
}

const Profile* CrosAppsApiFrameContext::Profile() const {
  return absl::visit(
      base::Overloaded{
          [](const raw_ref<content::RenderFrameHost> rfh) {
            return Profile::FromBrowserContext(rfh->GetBrowserContext());
          },
          [](const raw_ref<content::NavigationHandle> navigation_handle) {
            return Profile::FromBrowserContext(
                navigation_handle->GetWebContents()->GetBrowserContext());
          }},
      context_);
}
