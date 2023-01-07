// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/portal_task.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_task_provider.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/l10n/l10n_util.h"

namespace task_manager {

namespace {

std::u16string GetCurrentTitle(content::WebContents* web_contents) {
  DCHECK(web_contents);

  // Imitate the UI style of Subframe task.
  content::SiteInstance* site_instance =
      web_contents->GetPrimaryMainFrame()->GetSiteInstance();
  std::u16string site_url =
      base::UTF8ToUTF16(site_instance->GetSiteURL().spec());
  int message_id = site_instance->GetBrowserContext()->IsOffTheRecord()
                       ? IDS_TASK_MANAGER_PORTAL_INCOGNITO_PREFIX
                       : IDS_TASK_MANAGER_PORTAL_PREFIX;
  return l10n_util::GetStringFUTF16(message_id, site_url);
}

}  // namespace

PortalTask::PortalTask(content::WebContents* web_contents,
                       WebContentsTaskProvider* task_provider)
    : RendererTask(GetCurrentTitle(web_contents),
                   GetFaviconFromWebContents(web_contents),
                   web_contents),
      task_provider_(task_provider) {}

PortalTask::~PortalTask() = default;

void PortalTask::UpdateTitle() {
  set_title(GetCurrentTitle(web_contents()));
}

void PortalTask::UpdateFavicon() {
  const gfx::ImageSkia* icon = GetFaviconFromWebContents(web_contents());
  set_icon(icon ? *icon : gfx::ImageSkia());
}

void PortalTask::Activate() {
  content::WebContents* responsible_contents =
      web_contents()->GetResponsibleWebContents();
  if (auto* delegate = responsible_contents->GetDelegate())
    delegate->ActivateContents(responsible_contents);
}

const Task* PortalTask::GetParentTask() const {
  content::WebContents* responsible_contents =
      web_contents()->GetResponsibleWebContents();
  if (responsible_contents == web_contents())
    return nullptr;
  return task_provider_->GetTaskOfFrame(
      responsible_contents->GetPrimaryMainFrame());
}

}  // namespace task_manager
