// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_controller.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "chrome/browser/content_index/content_index_provider_impl.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/offline_item_model_manager.h"
#include "chrome/browser/download/offline_item_model_manager_factory.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/download/public/common/download_item.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "content/public/browser/download_manager.h"

using DownloadCreationType = ::download::DownloadItem::DownloadCreationType;

namespace {
constexpr int kShowDownloadsInBubbleForNumDays = 1;

bool FindOfflineItemByContentId(const ContentId& to_find,
                                const OfflineItem& candidate) {
  return candidate.id == to_find;
}

bool DownloadUIModelIsRecent(const DownloadUIModelPtr& model,
                             base::Time cutoff_time) {
  return ((model->GetStartTime().is_null() && !model->IsDone()) ||
          model->GetStartTime() > cutoff_time);
}

using DownloadUIModelPtrList = std::list<DownloadUIModelPtr>;
struct StartTimeComparator {
  bool operator()(const DownloadUIModelPtrList::iterator& a_iter,
                  const DownloadUIModelPtrList::iterator& b_iter) const {
    DownloadUIModel* a = (*a_iter).get();
    DownloadUIModel* b = (*b_iter).get();
    // Offline items have start time not populated, so display only not finished
    // ones.
    bool is_a_active_offline = (a->GetStartTime().is_null() && !a->IsDone());
    bool is_b_active_offline = (b->GetStartTime().is_null() && !b->IsDone());
    // a definitely shown before b if, 1) b is not an active offline item, and
    // 2) Either a is active offline item, or a is more recent
    return (!is_b_active_offline &&
            (is_a_active_offline || (a->GetStartTime() > b->GetStartTime())));
  }
};
using SortedDownloadUIModelSet =
    std::multiset<DownloadUIModelPtrList::iterator, StartTimeComparator>;

}  // namespace

DownloadBubbleUIController::DownloadBubbleUIController(Profile* profile)
    : profile_(profile),
      download_manager_(profile_->GetDownloadManager()),
      download_notifier_(download_manager_, this),
      aggregator_(OfflineContentAggregatorFactory::GetForKey(
          profile_->GetProfileKey())),
      offline_manager_(
          OfflineItemModelManagerFactory::GetForBrowserContext(profile_)) {
  observation_.Observe(aggregator_.get());
}

DownloadBubbleUIController::~DownloadBubbleUIController() = default;

bool DownloadBubbleUIController::MaybeAddOfflineItem(const OfflineItem& item,
                                                     bool is_new) {
  if (profile_->IsOffTheRecord() != item.is_off_the_record)
    return false;

  if (OfflineItemUtils::IsDownload(item.id))
    return false;

  if (item.state == OfflineItemState::CANCELLED)
    return false;

  if (item.id.name_space == ContentIndexProviderImpl::kProviderNamespace)
    return false;

  if (!OfflineItemModel::Wrap(offline_manager_, item)->ShouldShowInBubble())
    return false;

  offline_items_.push_back(item);
  if (is_new) {
    partial_view_ids_.insert(item.id);
  }
  return true;
}

void DownloadBubbleUIController::MaybeAddOfflineItems(
    base::OnceCallback<void()> callback,
    bool is_new,
    const OfflineItemList& offline_items) {
  for (const OfflineItem& item : offline_items) {
    MaybeAddOfflineItem(item, is_new);
  }
  std::move(callback).Run();
}

void DownloadBubbleUIController::InitOfflineItems(
    DownloadDisplayController* display_controller,
    base::OnceCallback<void()> callback) {
  display_controller_ = display_controller;
  aggregator_->GetAllItems(base::BindOnce(
      &DownloadBubbleUIController::MaybeAddOfflineItems,
      weak_factory_.GetWeakPtr(), std::move(callback), /*is_new=*/false));
}

const OfflineItemList& DownloadBubbleUIController::GetOfflineItems() {
  PruneOfflineItems();
  return offline_items_;
}

void DownloadBubbleUIController::OnManagerGoingDown(
    content::DownloadManager* manager) {
  if (manager == download_manager_) {
    download_manager_ = nullptr;
  }
}

void DownloadBubbleUIController::OnContentProviderGoingDown() {
  observation_.Reset();
}

void DownloadBubbleUIController::OnItemsAdded(
    const OfflineContentProvider::OfflineItemList& items) {
  bool any_new = false;
  bool any_in_progress = false;
  for (const OfflineItem& item : items) {
    if (MaybeAddOfflineItem(item, /*is_new=*/true)) {
      if (item.state == OfflineItemState::IN_PROGRESS) {
        any_in_progress = true;
      }
      any_new = true;
    }
  }
  if (any_new) {
    display_controller_->OnNewItem(any_in_progress);
  }
}

void DownloadBubbleUIController::OnDownloadCreated(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  DownloadUIModelPtr model = DownloadItemModel::Wrap(
      item, std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>());
  if (item && model->ShouldShowInBubble() &&
      model->download()->GetDownloadCreationType() !=
          DownloadCreationType::TYPE_HISTORY_IMPORT) {
    partial_view_ids_.insert(model->GetContentId());
    display_controller_->OnNewItem(item->GetState() ==
                                   download::DownloadItem::IN_PROGRESS);
  }
}

void DownloadBubbleUIController::OnItemRemoved(const ContentId& id) {
  offline_items_.erase(
      std::remove_if(offline_items_.begin(), offline_items_.end(),
                     [&id](const OfflineItem& candidate) {
                       return FindOfflineItemByContentId(id, candidate);
                     }),
      offline_items_.end());
  partial_view_ids_.erase(id);
  display_controller_->OnRemovedItem();
}

void DownloadBubbleUIController::OnDownloadRemoved(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  partial_view_ids_.erase(
      DownloadItemModel::Wrap(
          item, std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>())
          ->GetContentId());
  display_controller_->OnRemovedItem();
}

void DownloadBubbleUIController::OnItemUpdated(
    const OfflineItem& item,
    const absl::optional<UpdateDelta>& update_delta) {
  // Update item
  offline_items_.erase(
      std::remove_if(offline_items_.begin(), offline_items_.end(),
                     [&item](const OfflineItem& candidate) {
                       return FindOfflineItemByContentId(item.id, candidate);
                     }),
      offline_items_.end());
  MaybeAddOfflineItem(item, /*is_new=*/false);
  display_controller_->OnUpdatedItem(
      OfflineItemModel::Wrap(offline_manager_, item)->IsDone());
}

void DownloadBubbleUIController::OnDownloadUpdated(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  display_controller_->OnUpdatedItem(
      DownloadItemModel::Wrap(
          item, std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>())
          ->IsDone());
}

void DownloadBubbleUIController::RemoveContentIdFromPartialView(
    const ContentId& id) {
  partial_view_ids_.erase(id);
}

void DownloadBubbleUIController::PruneOfflineItems() {
  base::Time cutoff_time =
      base::Time::Now() - base::Days(kShowDownloadsInBubbleForNumDays);

  for (auto item_iter = offline_items_.begin();
       item_iter != offline_items_.end();) {
    if (!DownloadUIModelIsRecent(
            OfflineItemModel::Wrap(offline_manager_, *item_iter),
            cutoff_time)) {
      partial_view_ids_.erase(item_iter->id);
      item_iter = offline_items_.erase(item_iter);
    } else {
      item_iter++;
    }
  }
}

std::vector<DownloadUIModelPtr> DownloadBubbleUIController::GetDownloadUIModels(
    bool is_main_view) {
  // Prune just to keep the list of offline entries small.
  PruneOfflineItems();

  // Aggregate downloads and offline items
  std::vector<DownloadUIModelPtr> models_aggregate;
  for (const OfflineItem& item : offline_items_) {
    models_aggregate.push_back(OfflineItemModel::Wrap(
        offline_manager_, item,
        std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>()));
  }
  std::vector<download::DownloadItem*> download_items;
  download_manager_->GetAllDownloads(&download_items);
  for (download::DownloadItem* item : download_items) {
    models_aggregate.push_back(DownloadItemModel::Wrap(
        item, std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>()));
  }

  // Store list of DownloadUIModelPtrs. Sort list iterators in a set, as a set
  // does not allow move semantics over unique_ptr, preventing us from putting
  // DownloadUIModelPtr directly in the set.
  DownloadUIModelPtrList filtered_models_list;
  SortedDownloadUIModelSet sorted_ui_model_iters;
  base::Time cutoff_time =
      base::Time::Now() - base::Days(kShowDownloadsInBubbleForNumDays);
  for (auto& model : models_aggregate) {
    // Partial view consists of only the entries in partial_view_ids_, which are
    // also removed if viewed on the main view.
    if (model->ShouldShowInBubble() &&
        DownloadUIModelIsRecent(model, cutoff_time) &&
        (is_main_view || partial_view_ids_.find(model->GetContentId()) !=
                             partial_view_ids_.end())) {
      if (is_main_view) {
        partial_view_ids_.erase(model->GetContentId());
      }
      filtered_models_list.push_front(std::move(model));
      sorted_ui_model_iters.insert(filtered_models_list.begin());
    }
  }

  // Convert set iterators to sorted vector.
  std::vector<DownloadUIModelPtr> models_return_arr;
  for (const auto& model_iter : sorted_ui_model_iters) {
    models_return_arr.push_back(std::move((*model_iter)));
  }
  return models_return_arr;
}

std::vector<DownloadUIModelPtr> DownloadBubbleUIController::GetMainView() {
  return GetDownloadUIModels(/*is_main_view=*/true);
}

std::vector<DownloadUIModelPtr> DownloadBubbleUIController::GetPartialView() {
  return GetDownloadUIModels(/*is_main_view=*/false);
}
