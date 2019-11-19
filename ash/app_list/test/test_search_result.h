// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_TEST_TEST_SEARCH_RESULT_H_
#define ASH_APP_LIST_TEST_TEST_SEARCH_RESULT_H_

#include <memory>
#include <string>

#include "ash/app_list/model/search/search_result.h"
#include "base/macros.h"

namespace ash {

// A test search result which does nothing.
class TestSearchResult : public SearchResult {
 public:
  TestSearchResult();
  ~TestSearchResult() override;

  void set_result_id(const std::string& id);

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSearchResult);
};

}  // namespace ash

#endif  // ASH_APP_LIST_TEST_TEST_SEARCH_RESULT_H_
