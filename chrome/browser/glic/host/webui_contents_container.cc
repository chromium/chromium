// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/webui_contents_container.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic_ui.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#if !BUILDFLAG(IS_ANDROID)
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#endif
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui.h"
#include "printing/buildflags/buildflags.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/webview/webview.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/printing/printing_init.h"
#endif

namespace glic {

namespace {
content::WebContents::CreateParams MakeCreateParams(Profile* profile,
                                                    bool initially_hidden) {
  auto params = content::WebContents::CreateParams(profile);
  if (base::FeatureList::IsEnabled(
          features::kGlicGuestContentsVisibilityState)) {
    params.initially_hidden = initially_hidden;
  }
  return params;
}

}  // namespace

WebUIContentsContainer::WebUIContentsContainer()
    : creation_time_(base::TimeTicks::Now()) {}
WebUIContentsContainer::~WebUIContentsContainer() = default;

WebUIContentsContainerImpl::WebUIContentsContainerImpl(Profile* profile,
                                                       bool initially_hidden)
    : profile_keep_alive_(profile, ProfileKeepAliveOrigin::kGlicView),
      web_contents_(content::WebContents::Create(
          MakeCreateParams(profile, initially_hidden))),
      profile_(profile) {
  TRACE_EVENT_INSTANT("glic",
                      "WebUIContentsContainerImpl::WebUIContentsContainerImpl",
                      perfetto::Flow::FromPointer(this));
  CHECK(web_contents_);
  CreateGlicWebUiData(web_contents_.get());
  Observe(web_contents_.get());
  web_contents_->SetPageBaseBackgroundColor(
      web_contents_->GetColorProvider().GetColor(kColorGlicBackground));
  web_contents_->SetSupportsDraggableRegions(true);

#if !BUILDFLAG(IS_ANDROID)
  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      web_contents_.get());
#endif

#if BUILDFLAG(ENABLE_PRINTING)
  printing::InitializePrintingForWebContents(web_contents_.get());
#endif

  web_contents_->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(
          GURL{chrome::kChromeUIGlicURL}));
}

WebUIContentsContainerImpl::~WebUIContentsContainerImpl() {
  Observe(nullptr);
  web_contents_->ClosePage();
}

void WebUIContentsContainerImpl::AttachToHost(Host* host) {
  // This is only allowed to be called once.
  CHECK(!host_);
  host_ = host;
  if (auto* glic_ui = GlicUI::From(web_contents_.get())) {
    glic_ui->AttachToHost(host);
  }
}

void WebUIContentsContainerImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame()) {
    TRACE_EVENT_INSTANT(
        "glic",
        "WebUIContentsContainerImpl::DidFinishNavigation - PrimaryMainFrame",
        perfetto::Flow::FromPointer(this));
    navigation_commit_time_ = base::TimeTicks::Now();
    base::UmaHistogramTimes("Glic.Contents.NavigationCommitTime",
                            navigation_commit_time_ - creation_time_);
  }
  if (!host_ || !navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

#if BUILDFLAG(ENABLE_PRINTING)
  printing::InitializePrintingForWebContents(web_contents_.get());
#endif

  host_->OnWebContentsNavigated();

  // Re-attach to the (possibly new) GlicUI.
  if (auto* glic_ui = GlicUI::From(web_contents_.get())) {
    glic_ui->AttachToHost(host_);
  }
}

void WebUIContentsContainerImpl::PrimaryMainDocumentElementAvailable() {
  TRACE_EVENT_INSTANT(
      "glic", "WebUIContentsContainerImpl::PrimaryMainDocumentElementAvailable",
      perfetto::Flow::FromPointer(this));
}

void WebUIContentsContainerImpl::DocumentOnLoadCompletedInPrimaryMainFrame() {
  TRACE_EVENT_INSTANT(
      "glic",
      "WebUIContentsContainerImpl::DocumentOnLoadCompletedInPrimaryMainFrame",
      perfetto::Flow::FromPointer(this));
  base::UmaHistogramTimes("Glic.Contents.LoadCompleteTime",
                          base::TimeTicks::Now() - navigation_commit_time_);
}

void WebUIContentsContainerImpl::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  base::UmaHistogramEnumeration("Glic.Session.WebUiCrash.TerminationStatus",
                                status, base::TERMINATION_STATUS_MAX_ENUM);
  if (status != base::TERMINATION_STATUS_NORMAL_TERMINATION) {
    base::RecordAction(base::UserMetricsAction("GlicSessionWebUiCrash"));
  }
  auto* keyed_service = GlicKeyedServiceFactory::GetGlicKeyedService(profile_);
  // TODO(crbug.com/454120908): swap for a reloaded host in case of a crash.
  keyed_service->CloseAndShutdown(web_contents_->GetPrimaryMainFrame());
  // WARNING: Do not do any more work, as `this` may have been destroyed.
}

content::WebContents* WebUIContentsContainerImpl::web_contents() const {
  return web_contents_.get();
}

}  // namespace glic
