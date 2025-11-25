// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_distiller/tab_utils.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/dom_distiller/content/browser/distiller_page_web_contents.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/extraction_utils.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#endif

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
// if the WebContents navigates or goes away as well as if the distillation
// finishes. This class is a band-aid to keep a TaskTracker around until the
// distillation fininishes, and makes it to the cache. An optional callback can
// be provided which will be called when the article content is ready. The
// callback will be invoked with false if the object is destroyed before the
// callback is invoked.
class SelfDeletingRequestDelegate : public ViewRequestDelegate,
                                    public content::WebContentsObserver {
 public:
  explicit SelfDeletingRequestDelegate(
      content::WebContents* web_contents,
      std::optional<base::OnceCallback<void(bool)>> callback = std::nullopt);
  ~SelfDeletingRequestDelegate() override;
  void DeleteSelf();

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
  void SetCallback(base::OnceCallback<void(bool)> callback);

 private:
  // The handle to the view request towards the DomDistillerService. It
  // needs to be kept around to ensure the distillation request finishes.
  std::unique_ptr<ViewerHandle> viewer_handle_;
  std::optional<base::OnceCallback<void(bool)>> callback_;
  base::Time start_time_;
  bool deleting_ = false;
};

SelfDeletingRequestDelegate::SelfDeletingRequestDelegate(
    content::WebContents* web_contents,
    std::optional<base::OnceCallback<void(bool)>> callback)
    : WebContentsObserver(web_contents),
      callback_(std::move(callback)),
      start_time_(base::Time::Now()) {}

SelfDeletingRequestDelegate::~SelfDeletingRequestDelegate() = default;

void SelfDeletingRequestDelegate::DeleteSelf() {
  if (deleting_) {
    return;
  }
  deleting_ = true;

  // Ensure the callback is executed if the delegate is deleted before the
  // article distillation finishes (e.g. the user navigates away). The callback
  // is a OnceCallback, so if it has already been run, this will do nothing.
  if (callback_ && !callback_->is_null()) {
    std::move(callback_.value()).Run(false);
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

void SelfDeletingRequestDelegate::OnArticleReady(
    const DistilledArticleProto* article_proto) {
  if (callback_ && !callback_->is_null()) {
    bool has_content = article_proto != nullptr && article_proto->pages_size() > 0 &&
                       article_proto->pages(0).has_html() &&
                       !article_proto->pages(0).html().empty();
    std::move(callback_.value()).Run(has_content);
  }
  // Now that the work is done, always schedule for deletion.
  DeleteSelf();
}

void SelfDeletingRequestDelegate::OnArticleUpdated(
    ArticleDistillationUpdate article_update) {}

void SelfDeletingRequestDelegate::PrimaryPageChanged(content::Page& page) {
  Observe(nullptr);
  DeleteSelf();
}

void SelfDeletingRequestDelegate::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  Observe(nullptr);
  DeleteSelf();
}

void SelfDeletingRequestDelegate::WebContentsDestroyed() {
  Observe(nullptr);
  DeleteSelf();
}

void SelfDeletingRequestDelegate::TakeViewerHandle(
    std::unique_ptr<ViewerHandle> viewer_handle) {
  viewer_handle_ = std::move(viewer_handle);
}

void SelfDeletingRequestDelegate::SetCallback(
    base::OnceCallback<void(bool)> callback) {
  callback_ = std::move(callback);
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
  // For non-distillable URLs, return an empty article so the request is
  // fulfilled and the delegate is deleted.
  if (!dom_distiller::url_utils::IsUrlDistillable(last_committed_url)) {
    view_request_delegate->OnArticleReady(nullptr);
    return;
  }

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

void OnReadabilityHeuristicResult(base::OnceCallback<void(bool)> callback,
                                  base::Value value) {
  std::move(callback).Run(value.GetIfBool().value_or(false));
}

}  // namespace

void DistillCurrentPageAndViewIfSuccessful(
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(web_contents);
  SelfDeletingRequestDelegate* view_request_delegate =
      new SelfDeletingRequestDelegate(
          web_contents,
          base::BindOnce(
              [](base::OnceCallback<void(bool)> callback,
                 content::WebContents* web_contents, bool success) {
                std::move(callback).Run(success);
                if (success) {
                  StartNavigationToDistillerViewer(
                      web_contents, web_contents->GetLastCommittedURL());
                }
              },
              std::move(callback), web_contents));

  std::unique_ptr<SourcePageHandleWebContents> source_page_handle(
      new SourcePageHandleWebContents(web_contents, false));

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

void RunReadabilityHeuristicsOnWebContents(
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> callback) {
  std::string script = dom_distiller::GetReadabilityTriggeringScript();
  web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptInIsolatedWorld(
      base::UTF8ToUTF16(script),
      base::BindOnce(OnReadabilityHeuristicResult, std::move(callback)),
      ISOLATED_WORLD_ID_CHROME_INTERNAL);
}

