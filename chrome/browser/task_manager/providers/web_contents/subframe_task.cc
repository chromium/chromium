// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/subframe_task.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/url_identity.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "ui/base/l10n/l10n_util.h"

namespace task_manager {

namespace {

// Expected URL types for `UrlIdentity::CreateFromUrl(`.
constexpr UrlIdentity::TypeSet kUrlIdentityAllowedTypes = {
    UrlIdentity::Type::kDefault, UrlIdentity::Type::kFile,
    UrlIdentity::Type::kIsolatedWebApp, UrlIdentity::Type::kChromeExtension};
constexpr UrlIdentity::FormatOptions kUrlIdentityOptions = {
    .default_options = {UrlIdentity::DefaultFormatOptions::kRawSpec}};

}  // namespace

SubframeTask::SubframeTask(content::RenderFrameHost* render_frame_host,
                           RendererTask* main_task)
    : RendererTask(std::u16string(), nullptr, render_frame_host),
      site_instance_(render_frame_host->GetSiteInstance()),
      main_task_(main_task) {
  set_title(GetTitle());
  // Note that we didn't get the RenderProcessHost from the WebContents, but
  // rather from the RenderFrameHost. Out-of-process iframes reside on
  // different processes than that of their main frame.
}

SubframeTask::~SubframeTask() {
}

void SubframeTask::UpdateTitle() {
  set_title(GetTitle());
}

void SubframeTask::UpdateFavicon() {
  // This will be called when the favicon changes on the WebContents's main
  // frame, but this Task represents other frames, so we don't care.
}

Task* SubframeTask::GetParentTask() const {
  return main_task_;
}

void SubframeTask::Activate() {
  // Activate the root task.
  main_task_->Activate();
}

std::u16string SubframeTask::GetTitle() {
  DCHECK(site_instance_);

  // Subframe rows display the UrlIdentity of the SiteInstance URL.
  //
  // By default, subframe rows display the site, like this:
  //     "Subframe: http://example.com/"
  // For Extensions, subframe rows display extension name:
  //     "Subframe: Example Extension"
  // For Isolated Web Apps, subframe rows display IWA name:
  //     "Subframe: Example Isolated Web App"

  const GURL& site_url = site_instance_->GetSiteURL();
  Profile* profile =
      Profile::FromBrowserContext(site_instance_->GetBrowserContext());

  int message_id = profile->IsOffTheRecord()
                       ? IDS_TASK_MANAGER_SUBFRAME_INCOGNITO_PREFIX
                       : IDS_TASK_MANAGER_SUBFRAME_PREFIX;
  return l10n_util::GetStringFUTF16(
      message_id,
      UrlIdentity::CreateFromUrl(profile, site_url, kUrlIdentityAllowedTypes,
                                 kUrlIdentityOptions)
          .name);
}

}  // namespace task_manager
