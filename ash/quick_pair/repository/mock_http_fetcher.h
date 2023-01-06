// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_MOCK_HTTP_FETCHER_H_
#define ASH_QUICK_PAIR_REPOSITORY_MOCK_HTTP_FETCHER_H_

#include "ash/quick_pair/repository/http_fetcher.h"

#include "base/functional/callback.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace ash {
namespace quick_pair {

class MockHttpFetcher : public HttpFetcher {
 public:
  MockHttpFetcher();
  MockHttpFetcher(const MockHttpFetcher&) = delete;
  MockHttpFetcher& operator=(const MockHttpFetcher&) = delete;
  ~MockHttpFetcher() override;

  MOCK_METHOD(void,
              ExecuteGetRequest,
              (const GURL&, FetchCompleteCallback),
              (override));
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_MOCK_HTTP_FETCHER_H_
