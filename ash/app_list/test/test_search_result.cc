// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/test/test_search_result.h"

namespace ash {

TestSearchResult::TestSearchResult() = default;

TestSearchResult::~TestSearchResult() = default;

void TestSearchResult::set_result_id(const std::string& id) {
  set_id(id);
}

}  // namespace ash
