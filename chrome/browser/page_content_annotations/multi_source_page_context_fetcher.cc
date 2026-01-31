// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/page_content_annotations/page_content_screenshot_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace page_content_annotations {

void FetchPageContext(
    content::WebContents& web_contents,
    const FetchPageContextOptions& options,
    std::unique_ptr<FetchPageProgressListener> progress_listener,
    FetchPageContextResultCallback callback) {
  CHECK(callback);
  auto self = std::make_unique<PageContextFetcher>(
      base::BindRepeating([](content::BrowserContext* context) {
        return PageContentScreenshotServiceFactory::GetForProfile(
            Profile::FromBrowserContext(context));
      }),
      std::move(progress_listener));
  auto* raw_self = self.get();
  raw_self->FetchStart(web_contents, options,
                       base::BindOnce(
                           [](std::unique_ptr<PageContextFetcher> fetcher,
                              FetchPageContextResultCallback callback,
                              FetchPageContextResultCallbackArg result) {
                             std::move(callback).Run(std::move(result));
                           },
                           std::move(self), std::move(callback)));
}

}  // namespace page_content_annotations
