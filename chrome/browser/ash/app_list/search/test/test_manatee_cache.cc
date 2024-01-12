// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/test/test_manatee_cache.h"

namespace app_list {

TestManateeCache::TestManateeCache() : ManateeCache(nullptr, nullptr) {}

TestManateeCache::~TestManateeCache() = default;

void TestManateeCache::UrlLoader(std::vector<std::string> messages) {
  std::move(results_callback_).Run(response_);
}

void TestManateeCache::SetResponseForTest(EmbeddingsList embeddings) {
  response_ = embeddings;
}

}  // namespace app_list
