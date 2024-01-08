// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_SEARCH_PROVIDER_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"

namespace app_list {

class AppSearchDataSource;

class AppSearchProvider : public SearchProvider {
 public:
  explicit AppSearchProvider(AppSearchDataSource* data_source);

  AppSearchProvider(const AppSearchProvider&) = delete;
  AppSearchProvider& operator=(const AppSearchProvider&) = delete;

  ~AppSearchProvider() override;

  // SearchProvider overrides:
  void Start(const std::u16string& query) override;
  void StopQuery() override;
  ash::AppListSearchResultType ResultType() const override;

 private:
  void UpdateResults();

  const raw_ptr<AppSearchDataSource, DanglingUntriaged> data_source_;

  std::u16string query_;
  base::TimeTicks query_start_time_;
  bool record_query_uma_ = false;

  // Used to skip result updates caused by data source changes due to an
  // explicit refresh request.
  bool updates_blocked_ = false;

  base::CallbackListSubscription app_updates_subscription_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_SEARCH_PROVIDER_H_
