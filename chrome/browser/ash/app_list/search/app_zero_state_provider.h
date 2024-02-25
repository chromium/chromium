// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_ZERO_STATE_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_ZERO_STATE_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"

namespace app_list {

class AppSearchDataSource;

class AppZeroStateProvider : public SearchProvider {
 public:
  explicit AppZeroStateProvider(AppSearchDataSource* data_source);

  AppZeroStateProvider(const AppZeroStateProvider&) = delete;
  AppZeroStateProvider& operator=(const AppZeroStateProvider&) = delete;

  ~AppZeroStateProvider() override;

  // SearchProvider overrides:
  void StartZeroState() override;
  ash::AppListSearchResultType ResultType() const override;

 private:
  const raw_ptr<AppSearchDataSource, DanglingUntriaged> data_source_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_ZERO_STATE_PROVIDER_H_
