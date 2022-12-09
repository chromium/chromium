// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/search_provider.h"

#include <utility>

#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"

namespace app_list {

SearchProvider::SearchProvider() {}
SearchProvider::~SearchProvider() {}

void SearchProvider::SwapResults(Results* new_results) {
  Results results;
  results.swap(*new_results);
  if (search_controller_)
    search_controller_->SetResults(this, std::move(results));
}

}  // namespace app_list
