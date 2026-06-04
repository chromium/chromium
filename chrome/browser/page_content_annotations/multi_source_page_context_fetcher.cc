// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/page_content_annotations/page_content_screenshot_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "components/page_content_annotations/content/page_context_fetcher_manager.h"
#include "content/public/browser/web_contents.h"

namespace page_content_annotations {

void FetchPageContext(
    content::WebContents& web_contents,
    const FetchPageContextOptions& options,
    std::unique_ptr<FetchPageProgressListener> progress_listener,
    FetchPageContextResultCallback callback) {
  CHECK(callback);
  auto* manager =
      PageContextFetcherManager::GetOrCreateForWebContents(&web_contents);

  auto get_screenshot_service_callback =
      base::BindRepeating([](content::BrowserContext* context) {
        return PageContentScreenshotServiceFactory::GetForProfile(
            Profile::FromBrowserContext(context));
      });

  manager->Fetch(options, std::move(progress_listener),
                 std::move(get_screenshot_service_callback),
                 std::move(callback));
}

}  // namespace page_content_annotations
