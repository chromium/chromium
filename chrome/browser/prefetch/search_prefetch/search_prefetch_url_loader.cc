// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/search_prefetch/search_prefetch_url_loader.h"

#include "base/metrics/histogram_macros.h"

SearchPrefetchURLLoader::RequestHandler
SearchPrefetchURLLoader::ServingResponseHandler(
    std::unique_ptr<SearchPrefetchURLLoader> loader) {
  interception_time_ = base::TimeTicks::Now();
  return ServingResponseHandlerImpl(std::move(loader));
}

void SearchPrefetchURLLoader::OnForwardingComplete() {
  UMA_HISTOGRAM_TIMES(
      "Omnibox.SearchPrefetch.NavigationInterceptedToForwardingComplete",
      base::TimeTicks::Now() - interception_time_);
}
