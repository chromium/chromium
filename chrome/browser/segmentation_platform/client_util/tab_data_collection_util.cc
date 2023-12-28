// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/client_util/tab_data_collection_util.h"

#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/scoped_observation.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#include "components/segmentation_platform/embedder/input_delegate/tab_rank_dispatcher.h"
#include "components/segmentation_platform/embedder/tab_fetcher.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

namespace segmentation_platform {

class TabDataCollectionUtil::LocalTabModelObserver : public TabModelObserver {
 public:
  explicit LocalTabModelObserver(TabDataCollectionUtil* collection_util)
      : collection_util_(collection_util) {}
  ~LocalTabModelObserver() override = default;

  void DidSelectTab(TabAndroid* tab, TabModel::TabSelectionType type) override {
    collection_util_->OnTabAction(tab, TabAction::kTabSelected);
  }
  void TabPendingClosure(TabAndroid* tab) override {
    collection_util_->OnTabAction(tab, TabAction::kTabClose);
  }
  void AllTabsPendingClosure(
      const std::vector<raw_ptr<TabAndroid, VectorExperimental>>& tabs)
      override {
    for (const TabAndroid* tab : tabs) {
      collection_util_->OnTabAction(tab, TabAction::kTabClose);
    }
  }

 private:
  const raw_ptr<TabDataCollectionUtil> collection_util_;
};

class TabDataCollectionUtil::LocalTabModelListObserver
    : public TabModelListObserver {
 public:
  LocalTabModelListObserver(LocalTabModelObserver* tab_observer,
                            TabDataCollectionUtil* collection_util)
      : tab_observer_(tab_observer), collection_util_(collection_util) {
    ResetObservers();
  }
  ~LocalTabModelListObserver() override = default;

  void OnTabModelAdded() override { ResetObservers(); }
  void OnTabModelRemoved() override { ResetObservers(); }

  void ResetObservers() {
    observing_models_.clear();
    for (TabModel* model : TabModelList::models()) {
      observing_models_.emplace_back(
          std::make_unique<TabModelObservation>(tab_observer_.get()));
      observing_models_.back()->Observe(model);
    }
  }

 private:
  using TabModelObservation =
      base::ScopedObservation<TabModel, TabModelObserver>;
  const raw_ptr<LocalTabModelObserver> tab_observer_;
  const raw_ptr<TabDataCollectionUtil> collection_util_;
  std::vector<std::unique_ptr<TabModelObservation>> observing_models_;
};

TabDataCollectionUtil::TabDataCollectionUtil(
    SegmentationPlatformService* segmentation_service,
    TabRankDispatcher* tab_rank_dispatcher)
    : segmentation_service_(segmentation_service),
      tab_rank_dispatcher_(tab_rank_dispatcher),
      tab_observer_(std::make_unique<LocalTabModelObserver>(this)),
      tab_model_list_observer_(
          std::make_unique<LocalTabModelListObserver>(tab_observer_.get(),
                                                      this)) {
  TabModelList::AddObserver(tab_model_list_observer_.get());
  base::AddActionCallback(base::BindRepeating(
      &TabDataCollectionUtil::OnUserAction, weak_ptr_factory_.GetWeakPtr()));
}

TabDataCollectionUtil::~TabDataCollectionUtil() {
  TabModelList::RemoveObserver(tab_model_list_observer_.get());
}

void TabDataCollectionUtil::OnTabAction(const TabAndroid* tab,
                                        TabAction tab_action) {
  auto it = tab_requests_.find(tab);
  if (it != tab_requests_.end()) {
    TrainingLabels labels;
    labels.output_metric =
        std::make_pair("TabAction", static_cast<int>(tab_action));
    segmentation_service_->CollectTrainingData(
        proto::SegmentId::TAB_RESUMPTION_CLASSIFIER, it->second, labels,
        base::DoNothing());
  }
}

void TabDataCollectionUtil::OnUserAction(const std::string& action,
                                         base::TimeTicks time) {
  if (action != "MobileToolbarShowStackView") {
    return;
  }
  // Filter only local tabs, with TabAndroid object usable.
  auto filter = base::BindRepeating(
      [](const TabFetcher::Tab& tab) { return !!tab.tab_android; });
  tab_rank_dispatcher_->GetTopRankedTabs(
      kTabResumptionClassifierKey, TabRankDispatcher::TabFilter(),
      base::BindOnce(&TabDataCollectionUtil::OnGetRankedTabs,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TabDataCollectionUtil::OnGetRankedTabs(
    bool success,
    std::multiset<TabRankDispatcher::RankedTab> ranked_tabs) {
  if (!success) {
    return;
  }
  tab_requests_.clear();
  auto* fetcher = tab_rank_dispatcher_->tab_fetcher();
  for (const auto& candidate : ranked_tabs) {
    auto tab = fetcher->FindTab(candidate.tab);
    if (tab.tab_android) {
      tab_requests_[tab.tab_android] = candidate.request_id;
    }
  }
}

}  // namespace segmentation_platform
