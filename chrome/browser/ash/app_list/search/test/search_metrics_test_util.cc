// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/test/search_metrics_test_util.h"

#include <optional>

namespace app_list::test {

Result CreateFakeResult(Type type, const std::string& id) {
  return Result(id, type, std::nullopt);
}

}  // namespace app_list::test
