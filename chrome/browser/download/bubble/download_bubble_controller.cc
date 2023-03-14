// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_controller.h"

#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_index/content_index_provider_impl.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/bubble/download_bubble_ui_model_utils.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/download/offline_item_model_manager.h"
#include "chrome/browser/download/offline_item_model_manager_factory.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_stats.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "content/public/browser/download_manager.h"

using DownloadCreationType = ::download::DownloadItem::DownloadCreationType;

namespace {
constexpr int kShowDownloadsInBubbleForNumDays = 1;
constexpr int kMaxDownloadsToShow = 100;
// Don't show the partial view more than once per 15 seconds, as this pops up
// automatically and may be annoying to the user. The time is reset when the
// user clicks on the button to open the main view.
constexpr base::TimeDelta kShowPartialViewMinInterval = base::Seconds(15);
// Don't show the "download started" animation/UI for an extension or theme
// (crx) download until 2 seconds after it has begun. If it is a small download
// that finishes in under 2 seconds, the download UI does not show at all. If it
// is a large download that takes longer than 2 seconds, show the UI so that the
// user knows Chrome is working on it.
constexpr base::TimeDelta kCrxShowNewItemDelay = base::Seconds(2);
// Limit the size of the |delayed_crx_guids_| set so it doesn't grow
// unboundedly. It is unlikely that the user would have 20 active crx downloads
// simultaneously.
constexpr int kMaxDelayedCrxGuids = 20;

bool FindOfflineItemByContentId(const ContentId& to_find,
                                const OfflineItem& candidate) {
  return candidate.id == to_find;
}

using DownloadUIModelPtrList = std::list<DownloadUIModelPtr>;

// Sorting order is 1) Active in-progress downloads, 2) Paused in-progress
// downloads, 3) Other downloads
int GetSortOrder(DownloadUIModel* a) {
  if (a->GetState() == download::DownloadItem::IN_PROGRESS) {
    return a->IsPaused() ? 2 : 1;
  }
  return 3;
}

struct StartTimeComparator {
  bool operator()(const DownloadUIModelPtrList::iterator& a_iter,
                  const DownloadUIModelPtrList::iterator& b_iter) const {
    DownloadUIModel* a = (*a_iter).get();
    DownloadUIModel* b = (*b_iter).get();
    int a_sort_order = GetSortOrder(a);
    int b_sort_order = GetSortOrder(b);
    if (a_sort_order < b_sort_order) {
      return true;
    } else if (a_sort_order > b_sort_order) {
      return false;
    } else {
      // For the same sort order, sub-order by reverse chronological order.
      return (a->GetStartTime() > b->GetStartTime());
    }
  }
};
using SortedDownloadUIModelSet =
    std::multiset<DownloadUIModelPtrList::iterator, StartTimeComparator>;

void MaybeAddModel(DownloadUIModelPtr model,
                   base::Time cutoff_time,
                   DownloadUIModelPtrList& models_aggregate,
                   SortedDownloadUIModelSet& sorted_ui_model_iters) {
  if (model->ShouldShowInBubble() &&
      DownloadUIModelIsRecent(model.get(), cutoff_time)) {
    models_aggregate.push_front(std::move(model));
    sorted_ui_model_iters.insert(models_aggregate.begin());
  }
}

}  // namespace

DownloadBubbleUIController::DownloadBubbleUIController(Browser* browser)
    : browser_(browser),
      profile_(browser->profile()),
      download_manager_(profile_->GetDownloadManager()),
      download_notifier_(download_manager_, this),
      aggregator_(OfflineContentAggregatorFactory::GetForKey(
          profile_->GetProfileKey())),
      offline_manager_(
          OfflineItemModelManagerFactory::GetForBrowserContext(profile_)) {
  if (profile_->IsOffTheRecord()) {
    Profile* original_profile = profile_->GetOriginalProfile();
    original_notifier_ = std::make_unique<download::AllDownloadItemNotifier>(
        original_profile->GetDownloadManager(), this);
  }
  observation_.Observe(aggregator_.get());
}

DownloadBubbleUIController::~DownloadBubbleUIController() = default;

void DownloadBubbleUIController::HideDownloadUi() {
  display_controller_->HideToolbarButton();
}

void DownloadBubbleUIController::HandleButtonPressed() {
  RecordDownloadBubbleInteraction();
  display_controller_->HandleButtonPressed();
}

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

  OfflineItemModel model(offline_manager_, item);
  if (!model.ShouldShowInBubble()) {
    return false;
  }

  offline_items_.push_back(item);
  if (is_new && model.ShouldNotifyUI()) {
    model.SetActionedOn(false);
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

const std::vector<download::DownloadItem*>
DownloadBubbleUIController::GetDownloadItems() {
  std::vector<download::DownloadItem*> download_items;
  download_manager_->GetAllDownloads(&download_items);
  if (original_notifier_) {
    original_notifier_->GetManager()->GetAllDownloads(&download_items);
  }
  return download_items;
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
  for (const OfflineItem& item : items) {
    if (MaybeAddOfflineItem(item, /*is_new=*/true)) {
      any_new = true;
    }
  }
  if (any_new) {
    display_controller_->OnNewItem(/*show_animation=*/false);
  }
}

void DownloadBubbleUIController::OnNewItem(download::DownloadItem* item,
                                           bool may_show_animation) {
  if (download_crx_util::IsExtensionDownload(*item) &&
      delayed_crx_guids_.size() < kMaxDelayedCrxGuids) {
    const std::string& guid = item->GetGuid();
    DCHECK(!delayed_crx_guids_.contains(guid));
    delayed_crx_guids_.insert(guid);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DownloadBubbleUIController::OnDelayedNewItemByGuid,
                       weak_factory_.GetWeakPtr(), guid, may_show_animation),
        kCrxShowNewItemDelay);
    return;
  }
  DoOnNewItem(item, may_show_animation);
}

void DownloadBubbleUIController::DoOnNewItem(download::DownloadItem* item,
                                             bool may_show_animation) {
  DownloadItemModel model(item);
  if (model.ShouldNotifyUI()) {
    model.SetActionedOn(false);
  }
  display_controller_->OnNewItem(may_show_animation &&
                                 model.ShouldShowDownloadStartedAnimation());
}

void DownloadBubbleUIController::OnDelayedNewItemByGuid(
    const std::string& guid,
    bool may_show_animation) {
  // This assumes that for extension/theme downloads, the DownloadItem is
  // removed from the DownloadManager upon completion.
  download::DownloadItem* item = download_manager_->GetDownloadByGuid(guid);
  if (item && !item->IsDone()) {
    DoOnNewItem(item, may_show_animation);
  }
  size_t erased = delayed_crx_guids_.erase(guid);
  DCHECK_EQ(erased, 1u);
}

bool DownloadBubbleUIController::ShouldShowIncognitoIcon(
    const DownloadUIModel* model) const {
  return download::IsDownloadBubbleV2Enabled(profile_) &&
         model->GetDownloadItem() && model->GetDownloadItem()->IsOffTheRecord();
}

void DownloadBubbleUIController::OnItemRemoved(const ContentId& id) {
  if (OfflineItemUtils::IsDownload(id))
    return;
  offline_items_.erase(
      std::remove_if(offline_items_.begin(), offline_items_.end(),
                     [&id](const OfflineItem& candidate) {
                       return FindOfflineItemByContentId(id, candidate);
                     }),
      offline_items_.end());
  offline_manager_->RemoveOfflineItemModelData(id);
  display_controller_->OnRemovedItem(id);
}

void DownloadBubbleUIController::OnDownloadRemoved(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  std::make_unique<DownloadItemModel>(item)->SetActionedOn(true);
  const ContentId& id = OfflineItemUtils::GetContentIdForDownload(item);
  display_controller_->OnRemovedItem(id);
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
  bool was_added = MaybeAddOfflineItem(item, /*is_new=*/false);
  OfflineItemModel model(offline_manager_, item);
  display_controller_->OnUpdatedItem(
      model.IsDone(), IsPendingDeepScanning(&model),
      was_added &&
          (browser_ == chrome::FindLastActiveWithProfile(profile_.get())));
}

void DownloadBubbleUIController::OnDownloadUpdated(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  // If the item is an extension or theme download waiting out its 2-second
  // delay, don't show a UI update for it.
  if (delayed_crx_guids_.contains(item->GetGuid())) {
    return;
  }
  // manager can be different from download_notifier_ when the current profile
  // is off the record.
  DownloadItemModel model(item);
  if (manager != download_notifier_.GetManager()) {
    display_controller_->OnUpdatedItem(item->IsDone(),
                                       IsPendingDeepScanning(&model),
                                       /*may_show_details=*/false);
    return;
  }
  bool may_show_details =
      model.ShouldShowInBubble() &&
      (browser_ == chrome::FindLastActiveWithProfile(profile_.get()));
  display_controller_->OnUpdatedItem(
      item->IsDone(), IsPendingDeepScanning(&model), may_show_details);
}

void DownloadBubbleUIController::PruneOfflineItems() {
  base::Time cutoff_time =
      base::Time::Now() - base::Days(kShowDownloadsInBubbleForNumDays);

  for (auto item_iter = offline_items_.begin();
       item_iter != offline_items_.end();) {
    std::unique_ptr<DownloadUIModel> offline_model =
        std::make_unique<OfflineItemModel>(offline_manager_, *item_iter);
    if (!DownloadUIModelIsRecent(offline_model.get(), cutoff_time)) {
      offline_model->SetActionedOn(true);
      item_iter = offline_items_.erase(item_iter);
    } else {
      item_iter++;
    }
  }
}

std::vector<DownloadUIModelPtr>
DownloadBubbleUIController::GetAllItemsToDisplay() {
  base::Time cutoff_time =
      base::Time::Now() - base::Days(kShowDownloadsInBubbleForNumDays);

  // This list will contain all models, not limited to kMaxDownloadsToShow.
  // Must use a list, not a vector, because we are storing iterators which must
  // not be invalidated.
  DownloadUIModelPtrList models_aggregate;
  // Sort iterators into the above vector in a set, as a set does not allow
  // move semantics over unique_ptr, preventing us from putting
  // DownloadUIModelPtr directly in the set.
  SortedDownloadUIModelSet sorted_ui_model_iters;
  for (const OfflineItem& item : GetOfflineItems()) {
    DownloadUIModelPtr model = OfflineItemModel::Wrap(
        offline_manager_, item,
        std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>());
    MaybeAddModel(std::move(model), cutoff_time, models_aggregate,
                  sorted_ui_model_iters);
  }
  for (download::DownloadItem* item : GetDownloadItems()) {
    DownloadUIModelPtr model = DownloadItemModel::Wrap(
        item, std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>());
    MaybeAddModel(std::move(model), cutoff_time, models_aggregate,
                  sorted_ui_model_iters);
  }

  std::vector<DownloadUIModelPtr> items_to_display;
  if (models_aggregate.empty()) {
    return items_to_display;
  }

  DCHECK(!sorted_ui_model_iters.empty());
  SortedDownloadUIModelSet::const_iterator sorted_it =
      sorted_ui_model_iters.begin();
  for (size_t i = 0; i < kMaxDownloadsToShow; ++i) {
    DownloadUIModelPtrList::iterator model_it = *sorted_it;
    DCHECK(model_it != models_aggregate.end());
    items_to_display.push_back(std::move(*model_it));
    ++sorted_it;
    if (sorted_it == sorted_ui_model_iters.end()) {
      break;
    }
  }
  return items_to_display;
}

std::vector<DownloadUIModelPtr> DownloadBubbleUIController::GetDownloadUIModels(
    bool is_main_view) {
  // Prune just to keep the list of offline entries small.
  PruneOfflineItems();

  std::vector<DownloadUIModelPtr> all_items = GetAllItemsToDisplay();
  std::vector<DownloadUIModelPtr> items_to_return;
  for (auto& model : all_items) {
    if (!is_main_view && model->WasActionedOn()) {
      continue;
    }
    // Partial view entries are removed if viewed on the main view after
    // completion.
    if (is_main_view && !IsModelInProgress(model.get())) {
      model->SetActionedOn(true);
    }
    items_to_return.push_back(std::move(model));
  }
  return items_to_return;
}

std::vector<DownloadUIModelPtr> DownloadBubbleUIController::GetMainView() {
  if (last_partial_view_shown_time_.has_value()) {
    base::UmaHistogramLongTimes(
        "Download.Bubble.PartialToFullViewLatency",
        base::Time::Now() - (*last_partial_view_shown_time_));
    last_partial_view_shown_time_ = absl::nullopt;
  }
  std::vector<DownloadUIModelPtr> list =
      GetDownloadUIModels(/*is_main_view=*/true);
  base::UmaHistogramCounts100("Download.Bubble.FullViewSize", list.size());
  return list;
}

std::vector<DownloadUIModelPtr> DownloadBubbleUIController::GetPartialView() {
  base::Time now = base::Time::Now();
  if (last_partial_view_shown_time_.has_value() &&
      now - *last_partial_view_shown_time_ < kShowPartialViewMinInterval) {
    return {};
  }
  last_partial_view_shown_time_ = absl::make_optional(now);
  std::vector<DownloadUIModelPtr> list =
      GetDownloadUIModels(/*is_main_view=*/false);
  base::UmaHistogramCounts100("Download.Bubble.PartialViewSize", list.size());
  return list;
}

void DownloadBubbleUIController::ProcessDownloadButtonPress(
    DownloadUIModel* model,
    DownloadCommands::Command command,
    bool is_main_view) {
  RecordDownloadBubbleInteraction();
  DownloadCommands commands(model->GetWeakPtr());
  base::UmaHistogramExactLinear("Download.Bubble.ProcessedCommand", command,
                                DownloadCommands::MAX + 1);
  switch (command) {
    case DownloadCommands::KEEP:
    case DownloadCommands::DISCARD:
      DownloadItemWarningData::AddWarningActionEvent(
          model->GetDownloadItem(),
          is_main_view
              ? DownloadItemWarningData::WarningSurface::BUBBLE_MAINPAGE
              : DownloadItemWarningData::WarningSurface::BUBBLE_SUBPAGE,
          command == DownloadCommands::KEEP
              ? DownloadItemWarningData::WarningAction::PROCEED
              : DownloadItemWarningData::WarningAction::DISCARD);
      commands.ExecuteCommand(command);
      break;
    case DownloadCommands::REVIEW:
      model->ReviewScanningVerdict(
          browser_->tab_strip_model()->GetActiveWebContents());
      break;
    case DownloadCommands::RETRY:
      RetryDownload(model, command);
      break;
    case DownloadCommands::CANCEL:
      model->SetActionedOn(true);
      [[fallthrough]];
    case DownloadCommands::DEEP_SCAN:
    case DownloadCommands::BYPASS_DEEP_SCANNING:
    case DownloadCommands::RESUME:
    case DownloadCommands::PAUSE:
    case DownloadCommands::OPEN_WHEN_COMPLETE:
    case DownloadCommands::SHOW_IN_FOLDER:
    case DownloadCommands::ALWAYS_OPEN_TYPE:
      commands.ExecuteCommand(command);
      break;
    default:
      NOTREACHED() << "Unexpected button pressed on download bubble: "
                   << command;
  }
}

void DownloadBubbleUIController::RetryDownload(
    DownloadUIModel* model,
    DownloadCommands::Command command) {
  DCHECK(command == DownloadCommands::RETRY);
  display_controller_->HideBubble();
  RecordDownloadRetry(
      OfflineItemUtils::ConvertFailStateToDownloadInterruptReason(
          model->GetLastFailState()));

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("download_bubble_retry_download", R"(
        semantics {
          sender: "The download bubble"
          description: "Kick off retrying an interrupted download."
          trigger:
            "The user selects the retry button for an interrupted download on "
            "the downloads bubble."
          data: "None"
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "This feature cannot be disabled by settings, but it's only "
            "triggered by user request."
          policy_exception_justification: "Not implemented."
        })");

  // Use the last URL in the chain like resumption does.
  auto download_url_params = std::make_unique<download::DownloadUrlParameters>(
      model->GetURL(), traffic_annotation);
  // Set to false because user interaction is needed.
  download_url_params->set_content_initiated(false);
  download_url_params->set_download_source(
      download::DownloadSource::RETRY_FROM_BUBBLE);

  download_manager_->DownloadUrl(std::move(download_url_params));
}

void DownloadBubbleUIController::ScheduleCancelForEphemeralWarning(
    const std::string& guid) {
  DownloadCoreService* download_core_service =
      DownloadCoreServiceFactory::GetForBrowserContext(profile_);
  if (!download_core_service)
    return;
  ChromeDownloadManagerDelegate* delegate =
      download_core_service->GetDownloadManagerDelegate();
  if (delegate)
    delegate->ScheduleCancelForEphemeralWarning(guid);
}

void DownloadBubbleUIController::RecordDownloadBubbleInteraction() {
  download::SetShouldSuppressDownloadBubbleIph(
      browser_->profile()->GetOriginalProfile(), true);
}
