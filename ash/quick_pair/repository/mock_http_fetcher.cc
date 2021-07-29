// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/mock_http_fetcher.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace ash {
namespace quick_pair {

MockHttpFetcher::MockHttpFetcher()
    : HttpFetcher(TRAFFIC_ANNOTATION_FOR_TESTS) {}

MockHttpFetcher::MockHttpFetcher(
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : HttpFetcher(traffic_annotation) {}

MockHttpFetcher::~MockHttpFetcher() = default;

}  // namespace quick_pair
}  // namespace ash
