// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/arc/recommend_apps_fetcher.h"

#include "chrome/browser/ash/app_list/search/arc/recommend_apps_fetcher_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/storage_partition.h"

namespace app_list {

// static
std::unique_ptr<RecommendAppsFetcher> RecommendAppsFetcher::Create(
    RecommendAppsFetcherDelegate* delegate) {
  return std::make_unique<RecommendAppsFetcherImpl>(
      delegate, ProfileManager::GetActiveUserProfile()
                    ->GetDefaultStoragePartition()
                    ->GetURLLoaderFactoryForBrowserProcess()
                    .get());
}

}  // namespace app_list
