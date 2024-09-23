// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_distiller/tab_utils.h"

#include <utility>

#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/dom_distiller/content/browser/distiller_page_web_contents.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace {

using dom_distiller::ArticleDistillationUpdate;
using dom_distiller::DistilledArticleProto;
using dom_distiller::DistillerPage;
using dom_distiller::DomDistillerService;
using dom_distiller::DomDistillerServiceFactory;
using dom_distiller::SourcePageHandle;
using dom_distiller::SourcePageHandleWebContents;
using dom_distiller::ViewerHandle;
using dom_distiller::ViewRequestDelegate;

// An no-op ViewRequestDelegate which holds a ViewerHandle and deletes itself
// after the WebContents navigates or goes away. This class is a band-aid to
// keep a TaskTracker around until the distillation starts from the viewer.
class SelfDeletingRequestDelegate : public ViewRequestDelegate,
                                    public content::WebContentsObserver {
 public:
  explicit SelfDeletingRequestDelegate(content::WebContents* web_contents);
  ~SelfDeletingRequestDelegate() override;

  // ViewRequestDelegate implementation.
  void OnArticleReady(const DistilledArticleProto* article_proto) override;
  void OnArticleUpdated(ArticleDistillationUpdate article_update) override;

  // content::WebContentsObserver implementation.
  void PrimaryPageChanged(content::Page& page) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void WebContentsDestroyed() override;

  // Takes ownership of the ViewerHandle to keep distillation alive until |this|
  // is deleted.
  void TakeViewerHandle(std::unique_ptr<ViewerHandle> viewer_handle);

 private:
  // The handle to the view request towards the DomDistillerService. It
  // needs to be kept around to ensure the distillation request finishes.
  std::unique_ptr<ViewerHandle> viewer_handle_;
};

void SelfDeletingRequestDelegate::PrimaryPageChanged(content::Page& page) {
  Observe(nullptr);
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

void SelfDeletingRequestDelegate::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  Observe(nullptr);
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

void SelfDeletingRequestDelegate::WebContentsDestroyed() {
  Observe(nullptr);
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

SelfDeletingRequestDelegate::SelfDeletingRequestDelegate(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

SelfDeletingRequestDelegate::~SelfDeletingRequestDelegate() {}

void SelfDeletingRequestDelegate::OnArticleReady(
    const DistilledArticleProto* article_proto) {}

void SelfDeletingRequestDelegate::OnArticleUpdated(
    ArticleDistillationUpdate article_update) {}

void SelfDeletingRequestDelegate::TakeViewerHandle(
    std::unique_ptr<ViewerHandle> viewer_handle) {
  viewer_handle_ = std::move(viewer_handle);
}

// Start loading the viewer URL of the current page in |web_contents|.
void StartNavigationToDistillerViewer(content::WebContents* web_contents,
                                      const GURL& url) {
  GURL viewer_url = dom_distiller::url_utils::GetDistillerViewUrlFromUrl(
      dom_distiller::kDomDistillerScheme, url,
      base::UTF16ToUTF8(web_contents->GetTitle()),
      (base::TimeTicks::Now() - base::TimeTicks()).InMilliseconds());
  content::NavigationController::LoadURLParams params(viewer_url);
  params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  web_contents->GetController().LoadURLWithParams(params);
}

void MaybeStartDistillation(
    std::unique_ptr<SourcePageHandleWebContents> source_page_handle,
    SelfDeletingRequestDelegate* view_request_delegate) {
  const GURL& last_committed_url =
      source_page_handle->web_contents()->GetLastCommittedURL();
  if (!dom_distiller::url_utils::IsUrlDistillable(last_committed_url))
    return;

  // Disable back-forward cache when the distillation is in progress as it would
  // be cancelled and would not be restarted when the page is restored from the
  // cache.
  content::BackForwardCache::DisableForRenderFrameHost(
      source_page_handle->web_contents()->GetPrimaryMainFrame(),
      back_forward_cache::DisabledReason(
          back_forward_cache::DisabledReasonId::
              kDomDistiller_SelfDeletingRequestDelegate));

  // Start distillation using |source_page_handle|, and ensure ViewerHandle
  // stays around until the viewer requests distillation.
  DomDistillerService* dom_distiller_service =
      DomDistillerServiceFactory::GetForBrowserContext(
          source_page_handle->web_contents()->GetBrowserContext());
  std::unique_ptr<DistillerPage> distiller_page =
      dom_distiller_service->CreateDefaultDistillerPageWithHandle(
          std::move(source_page_handle));

  std::unique_ptr<ViewerHandle> viewer_handle = dom_distiller_service->ViewUrl(
      view_request_delegate, std::move(distiller_page), last_committed_url);
  view_request_delegate->TakeViewerHandle(std::move(viewer_handle));
}

}  // namespace

void DistillCurrentPageAndView(content::WebContents* old_web_contents) {
  DCHECK(old_web_contents);
  // Create new WebContents.
  content::WebContents::CreateParams create_params(
      old_web_contents->GetBrowserContext());
  std::unique_ptr<content::WebContents> new_web_contents =
      content::WebContents::Create(create_params);
  DCHECK(new_web_contents);

  // Copy all navigation state from the old WebContents to the new one.
  new_web_contents->GetController().CopyStateFrom(
      &old_web_contents->GetController(), /* needs_reload */ true);

  // StartNavigationToDistillerViewer must come before swapping the tab contents
  // to avoid triggering a reload of the page.  This reloadmakes it very
  // difficult to distinguish between the intermediate reload and a user hitting
  // the back button.
  StartNavigationToDistillerViewer(new_web_contents.get(),
                                   old_web_contents->GetLastCommittedURL());

  // This is used to start distillation and keep task_tracker alive till
  // main viewer is created.
  // Observes |new_web_contents| and is self deleted in the following cases
  // (whichever happens first).
  // 1. After navigation to distiller viewer is completed
  // 2. When |new_web_contents| is destroyed
  // 3. When render process attached to |new_web_contents| is gone
  // Observing new_web_contents instead of |old_web_contents| will make sure
  // that the destruction of |old_web_contents| will happen along with other
  // web_contents else we might end up caching it till browser close which will
  // lead to improper shutdown.
  // For more details refer - https://crbug.com/1221168
  SelfDeletingRequestDelegate* view_request_delegate =
      new SelfDeletingRequestDelegate(new_web_contents.get());

  TabAndroid* tab = TabAndroid::FromWebContents(old_web_contents);
  std::unique_ptr<content::WebContents> old_web_contents_owned =
      tab->SwapWebContents(std::move(new_web_contents),
                           /*did_start_load=*/false,
                           /*did_finish_load=*/false);

  std::unique_ptr<SourcePageHandleWebContents> source_page_handle(
      new SourcePageHandleWebContents(old_web_contents_owned.release(), true));

  MaybeStartDistillation(std::move(source_page_handle), view_request_delegate);
}

void DistillCurrentPage(content::WebContents* source_web_contents) {
  DCHECK(source_web_contents);

  std::unique_ptr<SourcePageHandleWebContents> source_page_handle(
      new SourcePageHandleWebContents(source_web_contents, false));

  SelfDeletingRequestDelegate* view_request_delegate =
      new SelfDeletingRequestDelegate(source_web_contents);

  MaybeStartDistillation(std::move(source_page_handle), view_request_delegate);
}

void DistillAndView(content::WebContents* source_web_contents,
                    content::WebContents* destination_web_contents) {
  DCHECK(destination_web_contents);

  DistillCurrentPage(source_web_contents);

  StartNavigationToDistillerViewer(destination_web_contents,
                                   source_web_contents->GetLastCommittedURL());
}
