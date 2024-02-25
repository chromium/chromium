// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEGMENTATION_PLATFORM_CLIENT_UTIL_TAB_DATA_COLLECTION_UTIL_H_
#define CHROME_BROWSER_SEGMENTATION_PLATFORM_CLIENT_UTIL_TAB_DATA_COLLECTION_UTIL_H_

#include <string>
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "components/segmentation_platform/embedder/input_delegate/tab_rank_dispatcher.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/segmentation_platform/public/trigger.h"

namespace segmentation_platform {

// Collection utility that ranks tabs and uploads training data about tab.
class TabDataCollectionUtil : public base::SupportsUserData::Data {
 public:
  TabDataCollectionUtil(SegmentationPlatformService* segmentation_service,
                        TabRankDispatcher* tab_rank_dispatcher);
  ~TabDataCollectionUtil() override;

  TabDataCollectionUtil(const TabDataCollectionUtil&) = delete;
  TabDataCollectionUtil& operator=(const TabDataCollectionUtil&) = delete;

 private:
  class LocalTabModelListObserver;
  class LocalTabModelObserver;

  // Represents the real output with action taken on the tab.
  enum TabAction : int {
    kUnknown,
    kTabSelected,
    kTabClose,
  };
  void OnTabAction(const TabAndroid* tab, TabAction tab_action);

  void OnUserAction(const std::string& action, base::TimeTicks time);

  void OnGetRankedTabs(bool success,
                       std::multiset<TabRankDispatcher::RankedTab> ranked_tabs);

  const raw_ptr<SegmentationPlatformService> segmentation_service_;
  const raw_ptr<TabRankDispatcher> tab_rank_dispatcher_;

  std::unique_ptr<LocalTabModelObserver> tab_observer_;
  std::unique_ptr<LocalTabModelListObserver> tab_model_list_observer_;

  std::map<const TabAndroid*, TrainingRequestId> tab_requests_;

  base::WeakPtrFactory<TabDataCollectionUtil> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // CHROME_BROWSER_SEGMENTATION_PLATFORM_CLIENT_UTIL_TAB_DATA_COLLECTION_UTIL_H_
