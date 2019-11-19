// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_distiller/tab_utils.h"

#include <utility>

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "components/dom_distiller/content/browser/distiller_page_web_contents.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
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
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void WebContentsDestroyed() override;

  // Takes ownership of the ViewerHandle to keep distillation alive until |this|
  // is deleted.
  void TakeViewerHandle(std::unique_ptr<ViewerHandle> viewer_handle);

 private:
  // The handle to the view request towards the DomDistillerService. It
  // needs to be kept around to ensure the distillation request finishes.
  std::unique_ptr<ViewerHandle> viewer_handle_;
};

void SelfDeletingRequestDelegate::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
    return;

  Observe(NULL);
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void SelfDeletingRequestDelegate::RenderProcessGone(
    base::TerminationStatus status) {
  Observe(NULL);
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void SelfDeletingRequestDelegate::WebContentsDestroyed() {
  Observe(NULL);
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
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
      (base::TimeTicks::Now() - base::TimeTicks()).InMilliseconds());
  content::NavigationController::LoadURLParams params(viewer_url);
  params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  web_contents->GetController().LoadURLWithParams(params);
}

void MaybeStartDistillation(
    std::unique_ptr<SourcePageHandleWebContents> source_page_handle) {
  const GURL& last_committed_url =
      source_page_handle->web_contents()->GetLastCommittedURL();
  if (!dom_distiller::url_utils::IsUrlDistillable(last_committed_url))
    return;

  // Start distillation using |source_page_handle|, and ensure ViewerHandle
  // stays around until the viewer requests distillation.
  SelfDeletingRequestDelegate* view_request_delegate =
      new SelfDeletingRequestDelegate(source_page_handle->web_contents());
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

  std::unique_ptr<content::WebContents> old_web_contents_owned =
      old_web_contents->GetDelegate()->SwapWebContents(
          old_web_contents, std::move(new_web_contents), false, false);

  std::unique_ptr<SourcePageHandleWebContents> source_page_handle(
      new SourcePageHandleWebContents(old_web_contents_owned.release(), true));

  MaybeStartDistillation(std::move(source_page_handle));
}

void DistillCurrentPage(content::WebContents* source_web_contents) {
  DCHECK(source_web_contents);

  std::unique_ptr<SourcePageHandleWebContents> source_page_handle(
      new SourcePageHandleWebContents(source_web_contents, false));

  MaybeStartDistillation(std::move(source_page_handle));
}

void DistillAndView(content::WebContents* source_web_contents,
                    content::WebContents* destination_web_contents) {
  DCHECK(destination_web_contents);

  DistillCurrentPage(source_web_contents);

  StartNavigationToDistillerViewer(destination_web_contents,
                                   source_web_contents->GetLastCommittedURL());
}

void ReturnToOriginalPage(content::WebContents* distilled_web_contents) {
  DCHECK(distilled_web_contents);
  DCHECK(dom_distiller::url_utils::IsDistilledPage(
      distilled_web_contents->GetLastCommittedURL()));

  GURL distilled_url = distilled_web_contents->GetLastCommittedURL();
  GURL source_url =
      dom_distiller::url_utils::GetOriginalUrlFromDistillerUrl(distilled_url);
  DCHECK_NE(source_url, distilled_url)
      << "Could not retrieve original page for distilled URL: "
      << distilled_url;

  // TODO(https://crbug.com/925965): Consider saving & retrieving the original
  // page web contents instead of reloading the page.
  content::NavigationController::LoadURLParams params(source_url);
  params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  distilled_web_contents->GetController().LoadURLWithParams(params);
}
