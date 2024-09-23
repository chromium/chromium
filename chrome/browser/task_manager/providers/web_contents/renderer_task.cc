// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/renderer_task.h"

#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/process_resource_usage.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "chrome/grit/generated_resources.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/base/l10n/l10n_util.h"

namespace task_manager {

namespace {

// Creates the Mojo service wrapper that will be used to sample the V8 memory
// usage and the the WebCache resource stats of the render process hosted by
// |render_process_host|.
ProcessResourceUsage* CreateRendererResourcesSampler(
    content::RenderProcessHost* render_process_host) {
  mojo::PendingRemote<content::mojom::ResourceUsageReporter> service;
  render_process_host->BindReceiver(service.InitWithNewPipeAndPassReceiver());
  return new ProcessResourceUsage(std::move(service));
}

// Gets the profile name associated with the browser context of the given
// |render_process_host| from the profile info cache.
std::u16string GetRendererProfileName(
    content::RenderProcessHost* render_process_host) {
  Profile* profile =
      Profile::FromBrowserContext(render_process_host->GetBrowserContext());
  return Task::GetProfileNameFromProfile(profile);
}

bool IsRendererResourceSamplingDisabled(int64_t flags) {
  return (flags & (REFRESH_TYPE_V8_MEMORY | REFRESH_TYPE_WEBCACHE_STATS)) == 0;
}

}  // namespace

RendererTask::RendererTask(const std::u16string& title,
                           const gfx::ImageSkia* icon,
                           content::WebContents* web_contents)
    : RendererTask(title,
                   icon,
                   web_contents,
                   web_contents->GetPrimaryMainFrame()->GetProcess()) {}

RendererTask::RendererTask(const std::u16string& title,
                           const gfx::ImageSkia* icon,
                           content::RenderFrameHost* subframe)
    : RendererTask(title,
                   icon,
                   content::WebContents::FromRenderFrameHost(subframe),
                   subframe->GetProcess()) {}

RendererTask::RendererTask(const std::u16string& title,
                           const gfx::ImageSkia* icon,
                           content::WebContents* web_contents,
                           content::RenderProcessHost* render_process_host)
    : Task(title, icon, render_process_host->GetProcess().Handle()),
      web_contents_(web_contents),
      render_process_host_(render_process_host),
      renderer_resources_sampler_(
          CreateRendererResourcesSampler(render_process_host_)),
      render_process_id_(render_process_host_->GetID()),
      v8_memory_allocated_(0),
      v8_memory_used_(0),
      webcache_stats_(blink::WebCacheResourceTypeStats()),
      profile_name_(GetRendererProfileName(render_process_host_)),
      termination_status_(base::TERMINATION_STATUS_STILL_RUNNING),
      termination_error_code_(0) {
  OnNetworkBytesRead(0);

  // Tag the web_contents with a |ContentFaviconDriver| (if needed) so that
  // we can use it to observe favicons changes.
  favicon::CreateContentFaviconDriverForWebContents(web_contents);
  favicon::ContentFaviconDriver::FromWebContents(web_contents)->AddObserver(
      this);
}

RendererTask::~RendererTask() {
  favicon::ContentFaviconDriver::FromWebContents(web_contents())->
      RemoveObserver(this);
}

void RendererTask::Activate() {
  if (!web_contents_->GetDelegate())
    return;

  web_contents_->GetDelegate()->ActivateContents(web_contents_);
}

void RendererTask::Refresh(const base::TimeDelta& update_interval,
                           int64_t refresh_flags) {
  Task::Refresh(update_interval, refresh_flags);

  if (IsRendererResourceSamplingDisabled(refresh_flags))
    return;

  // The renderer resources refresh is performed asynchronously, we will invoke
  // it and record the current values (which might be invalid at the moment. We
  // can safely ignore that and count on future refresh cycles potentially
  // having valid values).
  renderer_resources_sampler_->Refresh(base::DoNothing());

  v8_memory_allocated_ = base::saturated_cast<int64_t>(
      renderer_resources_sampler_->GetV8MemoryAllocated());
  v8_memory_used_ = base::saturated_cast<int64_t>(
      renderer_resources_sampler_->GetV8MemoryUsed());
  webcache_stats_ = renderer_resources_sampler_->GetBlinkMemoryCacheStats();
}

Task::Type RendererTask::GetType() const {
  return Task::RENDERER;
}

int RendererTask::GetChildProcessUniqueID() const {
  return render_process_id_;
}

void RendererTask::GetTerminationStatus(base::TerminationStatus* out_status,
                                        int* out_error_code) const {
  DCHECK(out_status);
  DCHECK(out_error_code);

  *out_status = termination_status_;
  *out_error_code = termination_error_code_;
}

std::u16string RendererTask::GetProfileName() const {
  return profile_name_;
}

SessionID RendererTask::GetTabId() const {
  return sessions::SessionTabHelper::IdForTab(web_contents_);
}

int64_t RendererTask::GetV8MemoryAllocated() const {
  return v8_memory_allocated_;
}

int64_t RendererTask::GetV8MemoryUsed() const {
  return v8_memory_used_;
}

bool RendererTask::ReportsWebCacheStats() const {
  return true;
}

blink::WebCacheResourceTypeStats RendererTask::GetWebCacheStats() const {
  return webcache_stats_;
}

void RendererTask::OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                                    NotificationIconType notification_icon_type,
                                    const GURL& icon_url,
                                    bool icon_url_changed,
                                    const gfx::Image& image) {
  if (notification_icon_type == NON_TOUCH_16_DIP)
    UpdateFavicon();
}

base::WeakPtr<RendererTask> RendererTask::AsWeakPtr() {
  return weak_ptr_factor_.GetWeakPtr();
}

// static
std::u16string RendererTask::GetTitleFromWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  std::u16string title = web_contents->GetTitle();
  if (title.empty()) {
    GURL url = web_contents->GetLastCommittedURL();
    title = base::UTF8ToUTF16(url.spec());
    // Force URL to be LTR.
    title = base::i18n::GetDisplayStringInLTRDirectionality(title);
  } else {
    // Since the title could later be concatenated with
    // IDS_TASK_MANAGER_TAB_PREFIX (for example), we need to explicitly set the
    // title to be LTR format if there is no strong RTL charater in it.
    // Otherwise, if IDS_TASK_MANAGER_TAB_PREFIX is an RTL word, the
    // concatenated result might be wrong. For example, http://mail.yahoo.com,
    // whose title is "Yahoo! Mail: The best web-based Email!", without setting
    // it explicitly as LTR format, the concatenated result will be "!Yahoo!
    // Mail: The best web-based Email :BAT", in which the capital letters "BAT"
    // stands for the Hebrew word for "tab".
    base::i18n::AdjustStringForLocaleDirection(&title);
  }
  return title;
}

// static
const gfx::ImageSkia* RendererTask::GetFaviconFromWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);

  // Tag the web_contents with a |ContentFaviconDriver| (if needed) so that
  // we can use it to retrieve the favicon if there is one.
  favicon::CreateContentFaviconDriverForWebContents(web_contents);
  gfx::Image image =
      favicon::ContentFaviconDriver::FromWebContents(web_contents)->
          GetFavicon();
  if (image.IsEmpty())
    return nullptr;

  return image.ToImageSkia();
}

// static
const std::u16string RendererTask::PrefixRendererTitle(
    const std::u16string& title,
    bool is_app,
    bool is_extension,
    bool is_incognito,
    bool is_background) {
  int message_id = IDS_TASK_MANAGER_TAB_PREFIX;

  if (is_incognito && !is_app && !is_extension) {
    message_id = IDS_TASK_MANAGER_TAB_INCOGNITO_PREFIX;
  } else if (is_app) {
    if (is_background)
      message_id = IDS_TASK_MANAGER_BACKGROUND_PREFIX;
    else if (is_incognito)
      message_id = IDS_TASK_MANAGER_APP_INCOGNITO_PREFIX;
    else
      message_id = IDS_TASK_MANAGER_APP_PREFIX;
  } else if (is_extension) {
    if (is_incognito)
      message_id = IDS_TASK_MANAGER_EXTENSION_INCOGNITO_PREFIX;
    else
      message_id = IDS_TASK_MANAGER_EXTENSION_PREFIX;
  }

  return l10n_util::GetStringFUTF16(message_id, title);
}

}  // namespace task_manager
