// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/accessibility_query_service_delegate_impl.h"

#include <utility>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/page_content_annotations/page_embeddings_service_factory.h"
#include "chrome/browser/passage_embeddings/chrome_passage_embeddings_service_controller.h"
#include "chrome/browser/profiles/profile.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#endif
#include "components/accessibility_annotator/content/live_tab_context/live_tab_retriever.h"
#include "components/accessibility_annotator/core/live_tab_context/search.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"

namespace accessibility_annotator {

using ::page_content_annotations::PageContentExtractionServiceFactory;
using ::page_content_annotations::PageEmbeddingsServiceFactory;
using ::passage_embeddings::ChromePassageEmbeddingsServiceController;
using ::passage_embeddings::Embedder;

namespace {
Embedder* GetEmbedder(Profile* profile) {
  ChromePassageEmbeddingsServiceController* controller =
      ChromePassageEmbeddingsServiceController::Get();
  return controller ? controller->GetEmbedder() : nullptr;
}
}  // namespace

// Production constructor that fetches service dependencies from profile.
AccessibilityQueryServiceDelegateImpl::AccessibilityQueryServiceDelegateImpl(
    Profile* profile)
    : AccessibilityQueryServiceDelegateImpl(
          profile,
          PageContentExtractionServiceFactory::GetForProfile(profile),
          PageEmbeddingsServiceFactory::GetForProfile(profile),
          GetEmbedder(profile)) {}

// Test constructor that allows service dependency injection.
AccessibilityQueryServiceDelegateImpl::AccessibilityQueryServiceDelegateImpl(
    Profile* profile,
    page_content_annotations::PageContentExtractionService* extraction_service,
    page_content_annotations::PageEmbeddingsService* embeddings_service,
    passage_embeddings::Embedder* embedder)
    : profile_(profile) {
  if (extraction_service && embeddings_service && embedder) {
    live_tab_retriever_ = std::make_unique<LiveTabRetriever>(
        *extraction_service, *embeddings_service, *embedder);
  }
}

AccessibilityQueryServiceDelegateImpl::
    ~AccessibilityQueryServiceDelegateImpl() = default;

void AccessibilityQueryServiceDelegateImpl::RetrieveLiveTabContext(
    LiveTabContextQuery query,
    base::OnceCallback<void(LiveTabContextResponse)> callback) {
  if (!live_tab_retriever_) {
    std::move(callback).Run({});
    return;
  }

  std::vector<content::WebContents*> tabs;

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/507505897): Implement Android support using TabModelList.
#else
  // Get tabs from all windows (Excluding non-profile windows, e.g. incognito)
  // TODO(crbug.com/488696556): Instead of fetching all tabs from all windows,
  // we should only fetch tabs relevant to the query.
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&](BrowserWindowInterface* browser_interface) {
        if (browser_interface->GetProfile() != profile_) {
          return true;
        }
        for (tabs::TabInterface* tab :
             browser_interface->GetAllTabInterfaces()) {
          if (content::WebContents* web_contents = tab->GetContents()) {
            tabs.push_back(web_contents);
          }
        }
        return true;
      });
#endif

  // Parse the LiveTabRetriever's ScoredPassages into a LiveTabContextResponse.
  live_tab_retriever_->Retrieve(
      query.query, tabs,
      base::BindOnce([](std::vector<ScoredPassage> passages) {
        LiveTabContextResponse response;
        response.results = base::ToVector(passages, &ScoredPassage::passage);
        return response;
      }).Then(std::move(callback)));
}

}  // namespace accessibility_annotator
