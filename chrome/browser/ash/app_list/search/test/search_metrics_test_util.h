// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_SEARCH_METRICS_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_SEARCH_METRICS_TEST_UTIL_H_

#include <string>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_notifier.h"

namespace app_list::test {

using Result = ash::AppListNotifier::Result;
using Location = ash::AppListNotifier::Location;
using Type = ash::SearchResultType;

Result CreateFakeResult(Type type, const std::string& id);

}  // namespace app_list::test

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_SEARCH_METRICS_TEST_UTIL_H_
