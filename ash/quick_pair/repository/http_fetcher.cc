// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/http_fetcher.h"

namespace ash {
namespace quick_pair {

HttpFetcher::HttpFetcher() = default;

HttpFetcher::~HttpFetcher() = default;

void HttpFetcher::ExecutePostRequest(const GURL& url,
                                     const std::string& body,
                                     FetchCompleteCallback callback) {
  NOTREACHED();
}

void HttpFetcher::ExecuteDeleteRequest(const GURL& url,
                                       FetchCompleteCallback callback) {
  NOTREACHED();
}

}  // namespace quick_pair
}  // namespace ash
