// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_TAB_CONTEXT_FETCHER_H_
#define CHROME_BROWSER_CONTEXT_HUB_TAB_CONTEXT_FETCHER_H_

#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/context_hub/tab_group_store/tab_group_entry.h"
#include "components/optimization_guide/proto/features/context_hub.pb.h"
#include "components/page_content_annotations/core/page_content_extraction_types.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class NavigationHandle;
class Page;
class WebContents;
}  // namespace content

namespace page_content_annotations {
class PageContentExtractionService;
}  // namespace page_content_annotations

namespace context_hub {

// Asynchronously loads and extracts annotated page content for a given tab.
// If the tab is unloaded/discarded or has an uncommitted navigation in
// progress, it waits for the navigation in the primary main frame to commit (or
// until `features::kTabLoadTimeout` expires) before requesting page content
// extraction. Post-commit page loading and settling is handled by
// PageContentExtractionService.
class TabContextFetcher : public content::WebContentsObserver {
 public:
  using TabContextResult =
      std::pair<std::optional<TabData>,
                std::optional<optimization_guide::proto::PageContext>>;
  using TabContextCallback = base::OnceCallback<void(TabContextResult)>;

  TabContextFetcher(page_content_annotations::PageContentExtractionService&
                        extraction_service,
                    content::WebContents* web_contents,
                    TabContextCallback callback);

  TabContextFetcher(const TabContextFetcher&) = delete;
  TabContextFetcher& operator=(const TabContextFetcher&) = delete;

  ~TabContextFetcher() override;

  // Initiates tab loading if necessary and waits for navigation commit or
  // timeout before starting page content extraction.
  void Start();

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

 private:
  void OnTimeout();
  void ExtractPageContent();
  void OnExtractionComplete(
      base::WeakPtr<content::Page> page,
      std::optional<page_content_annotations::ExtractedPageContentResult>
          extracted_result);
  void Finish(
      std::optional<TabData> tab,
      std::optional<optimization_guide::proto::PageContext> page_context);
  std::optional<TabData> GetTabData() const;

  const raw_ref<page_content_annotations::PageContentExtractionService>
      extraction_service_;
  TabContextCallback callback_;
  // Timer to enforce the timeout for waiting for navigation to commit.
  base::OneShotTimer timer_;
  // Ensures Start() is only executed once.
  bool started_ = false;
  // Ensures extraction is triggered at most once across timeout and
  // DidFinishNavigation events.
  bool extraction_started_ = false;
  base::WeakPtrFactory<TabContextFetcher> weak_factory_{this};
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_TAB_CONTEXT_FETCHER_H_
