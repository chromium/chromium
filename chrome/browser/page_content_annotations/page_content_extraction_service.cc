// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "chrome/browser/page_content_annotations/annotate_page_content_request.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_web_contents_observer.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_types.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_content_annotations/core/page_content_cache_handler.h"
#include "components/page_content_annotations/core/web_state_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"

namespace page_content_annotations {

namespace {

WebStateWrapper ToWebStateWrapper(content::WebContents* web_contents) {
  return WebStateWrapper(
      web_contents->GetBrowserContext()->IsOffTheRecord(),
      web_contents->GetLastCommittedURL(),
      web_contents->GetController().GetLastCommittedEntry()->GetTimestamp(),
      web_contents->GetVisibility() == content::Visibility::VISIBLE
          ? PageContentVisibility::kVisible
          : PageContentVisibility::kHidden);
}

optimization_guide::proto::PageContext ToPageContext(
    optimization_guide::proto::AnnotatedPageContent apc) {
  optimization_guide::proto::PageContext page_context;
  *page_context.mutable_annotated_page_content() = std::move(apc);
  return page_context;
}

}  // namespace

PageContentExtractionService::PageContentExtractionService(
    os_crypt_async::OSCryptAsync* os_crypt_async,
    const base::FilePath& profile_path)
    : is_page_content_cache_enabled_(
          base::FeatureList::IsEnabled(features::kPageContentCache)),
      page_content_cache_handler_(
          is_page_content_cache_enabled_
              ? std::make_unique<PageContentCacheHandler>(os_crypt_async,
                                                          profile_path)
              : nullptr) {}

PageContentExtractionService::~PageContentExtractionService() {
  ClearAllUserData();
}

void PageContentExtractionService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PageContentExtractionService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PageContentExtractionService::ShouldEnablePageContentExtraction() const {
  if (base::FeatureList::IsEnabled(page_content_annotations::features::
                                       kAnnotatedPageContentExtraction)) {
    return true;
  }
  return !observers_.empty();
}

void PageContentExtractionService::OnPageContentExtracted(
    content::Page& page,
    const optimization_guide::proto::AnnotatedPageContent& page_content,
    std::optional<int> tab_id) {
  for (auto& observer : observers_) {
    observer.OnPageContentExtracted(page, page_content);
  }

  if (!is_page_content_cache_enabled_) {
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&page.GetMainDocument());
  if (!web_contents) {
    return;
  }

  page_content_cache_handler_->ProcessPageContentExtraction(
      tab_id, ToWebStateWrapper(web_contents), ToPageContext(page_content));
}

std::optional<ExtractedPageContentResult>
PageContentExtractionService::GetExtractedPageContentAndEligibilityForPage(
    content::Page& page) {
  return GetCachedContentsFromWebContents(
      content::WebContents::FromRenderFrameHost(&page.GetMainDocument()));
}

void PageContentExtractionService::OnTabClosed(int64_t tab_id) {
  if (is_page_content_cache_enabled_) {
    page_content_cache_handler_->OnTabClosed(tab_id);
  }
}

void PageContentExtractionService::OnVisibilityChanged(
    std::optional<int64_t> tab_id,
    content::WebContents* web_contents,
    content::Visibility visibility) {
  if (is_page_content_cache_enabled_) {
    std::optional<ExtractedPageContentResult> extracted_result =
        GetCachedContentsFromWebContents(web_contents);
    if (extracted_result) {
      page_content_cache_handler_->OnVisibilityChanged(
          tab_id, ToWebStateWrapper(web_contents),
          ToPageContext(std::move(extracted_result->page_content)));
    }
  }
}

void PageContentExtractionService::OnNewNavigation(
    std::optional<int64_t> tab_id,
    content::WebContents* web_contents) {
  if (is_page_content_cache_enabled_) {
    page_content_cache_handler_->OnNewNavigation(
        tab_id, ToWebStateWrapper(web_contents));
  }
}

PageContentCache* PageContentExtractionService::GetPageContentCache() {
  return is_page_content_cache_enabled_
             ? page_content_cache_handler_->page_content_cache()
             : nullptr;
}

std::optional<ExtractedPageContentResult>
PageContentExtractionService::GetCachedContentsFromWebContents(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return std::nullopt;
  }
  PageContentAnnotationsWebContentsObserver* observer =
      PageContentAnnotationsWebContentsObserver::FromWebContents(web_contents);
  if (observer) {
    AnnotatedPageContentRequest* request =
        observer->GetAnnotatedPageContentRequest();
    if (request) {
      return request->GetCachedContentAndEligibility();
    }
  }
  return std::nullopt;
}

}  // namespace page_content_annotations
