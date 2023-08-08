// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_url_loader.h"

#include <utility>

#include "base/metrics/histogram_macros.h"

// static
SearchPrefetchURLLoader::RequestHandler
SearchPrefetchURLLoader::GetServingResponseHandlerFromLoader(
    std::unique_ptr<SearchPrefetchURLLoader> loader) {
  DCHECK(loader);
  loader->RecordInterceptionTime();
  auto* raw_pointer = loader.get();

  // Hand ownership of the loader to the callback, when the callback runs,
  // mojo connection termination will manage it. If the callback is deleted,
  // the loader will be deleted.
  return raw_pointer->ServingResponseHandlerImpl(std::move(loader));
}

void SearchPrefetchURLLoader::RecordInterceptionTime() {
  interception_time_ = base::TimeTicks::Now();
}

void SearchPrefetchURLLoader::OnForwardingComplete() {
  UMA_HISTOGRAM_TIMES(
      "Omnibox.SearchPrefetch.NavigationInterceptedToForwardingComplete",
      base::TimeTicks::Now() - interception_time_);
}
