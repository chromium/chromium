// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/essential_search/socs_cookie_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace app_list {

SocsCookieFetcher::SocsCookieFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Consumer* consumer)
    : consumer_(consumer), url_loader_factory_(std::move(url_loader_factory)) {
  DCHECK(consumer_);
}

SocsCookieFetcher::~SocsCookieFetcher() = default;

SocsCookieFetcher::Consumer::Consumer() = default;

SocsCookieFetcher::Consumer::~Consumer() = default;

void SocsCookieFetcher::StartFetching() {
  NOTIMPLEMENTED();
}

}  // namespace app_list
