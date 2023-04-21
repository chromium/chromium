// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"

#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_index/content_index_provider_impl.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/bubble/download_bubble_update_service.h"
#include "chrome/browser/download/bubble/download_bubble_update_service_factory.h"
#include "chrome/browser/download/bubble/download_bubble_utils.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/download/offline_item_model_manager.h"
#include "chrome/browser/download/offline_item_model_manager_factory.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_stats.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "content/public/browser/download_manager.h"

namespace {

using DownloadCreationType = ::download::DownloadItem::DownloadCreationType;
using DownloadUIModelPtr = DownloadUIModel::DownloadUIModelPtr;

// Don't show the partial view more than once per 15 seconds, as this pops up
// automatically and may be annoying to the user. The time is reset when the
// user clicks on the button to open the main view.
constexpr base::TimeDelta kShowPartialViewMinInterval = base::Seconds(15);

}  // namespace

DownloadBubbleUIController::DownloadBubbleUIController(Browser* browser)
    : DownloadBubbleUIController(
          browser,
          DownloadBubbleUpdateServiceFactory::GetForProfile(
              browser->profile())) {}

DownloadBubbleUIController::DownloadBubbleUIController(
    Browser* browser,
    DownloadBubbleUpdateService* update_service)
    : browser_(browser),
      profile_(browser->profile()),
      update_service_(update_service),
      offline_manager_(
          OfflineItemModelManagerFactory::GetForBrowserContext(profile_)) {}

DownloadBubbleUIController::~DownloadBubbleUIController() = default;

void DownloadBubbleUIController::HideDownloadUi() {
  display_controller_->HideToolbarButton();
}

void DownloadBubbleUIController::HandleButtonPressed() {
  RecordDownloadBubbleInteraction();
  display_controller_->HandleButtonPressed();
}

void DownloadBubbleUIController::OnOfflineItemsAdded(
    const OfflineContentProvider::OfflineItemList& items) {
  display_controller_->OnNewItem(/*show_animation=*/false);
}

void DownloadBubbleUIController::OnDownloadItemAdded(
    download::DownloadItem* item,
    bool may_show_animation) {
  DownloadItemModel model(item);
  if (model.ShouldNotifyUI()) {
    model.SetActionedOn(false);
  }
  display_controller_->OnNewItem(may_show_animation &&
                                 model.ShouldShowDownloadStartedAnimation());
}

bool DownloadBubbleUIController::ShouldShowIncognitoIcon(
    const DownloadUIModel* model) const {
  return download::IsDownloadBubbleV2Enabled(profile_) &&
         model->GetDownloadItem() && model->GetDownloadItem()->IsOffTheRecord();
}

void DownloadBubbleUIController::OnOfflineItemRemoved(const ContentId& id) {
  if (OfflineItemUtils::IsDownload(id)) {
    return;
  }
  offline_manager_->RemoveOfflineItemModelData(id);
  display_controller_->OnRemovedItem(id);
}

void DownloadBubbleUIController::OnDownloadItemRemoved(
    download::DownloadItem* item) {
  std::make_unique<DownloadItemModel>(item)->SetActionedOn(true);
  const ContentId& id = OfflineItemUtils::GetContentIdForDownload(item);
  display_controller_->OnRemovedItem(id);
}

void DownloadBubbleUIController::OnOfflineItemUpdated(const OfflineItem& item) {
  OfflineItemModel model(offline_manager_, item);
  bool may_show_details =
      model.ShouldShowInBubble() &&
      (browser_ == chrome::FindLastActiveWithProfile(profile_.get()));
  // Consider dangerous in-progress downloads to be completed.
  bool is_done = model.IsDone() ||
                 (model.GetState() == download::DownloadItem::IN_PROGRESS &&
                  !IsModelInProgress(&model));
  display_controller_->OnUpdatedItem(is_done, may_show_details);
}

void DownloadBubbleUIController::OnDownloadItemUpdated(
    download::DownloadItem* item) {
  DownloadItemModel model(item);
  bool may_show_details =
      model.ShouldShowInBubble() &&
      (browser_ == chrome::FindLastActiveWithProfile(profile_.get()));
  // Consider dangerous in-progress downloads to be completed.
  bool is_done = item->IsDone() ||
                 (item->GetState() == download::DownloadItem::IN_PROGRESS &&
                  !IsItemInProgress(item));
  display_controller_->OnUpdatedItem(is_done, may_show_details);
}

std::vector<DownloadUIModelPtr> DownloadBubbleUIController::GetDownloadUIModels(
    bool is_main_view) {
  std::vector<DownloadUIModelPtr> all_items;
  if (!update_service_->IsInitialized()) {
    return all_items;
  }
  update_service_->GetAllModelsToDisplay(
      all_items, /*force_backfill_download_items=*/true);
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
  if (!download::IsDownloadBubblePartialViewEnabled(profile_)) {
    return {};
  }
  std::vector<DownloadUIModelPtr> list =
      GetDownloadUIModels(/*is_main_view=*/false);
  if (!list.empty()) {
    last_partial_view_shown_time_ = absl::make_optional(now);
  }
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
  content::DownloadManager* download_manager = profile_->GetDownloadManager();
  if (!download_manager) {
    return;
  }
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

  download_manager->DownloadUrl(std::move(download_url_params));
}

void DownloadBubbleUIController::ScheduleCancelForEphemeralWarning(
    const std::string& guid) {
  DownloadCoreService* download_core_service =
      DownloadCoreServiceFactory::GetForBrowserContext(profile_);
  if (!download_core_service) {
    return;
  }
  ChromeDownloadManagerDelegate* delegate =
      download_core_service->GetDownloadManagerDelegate();
  if (delegate) {
    delegate->ScheduleCancelForEphemeralWarning(guid);
  }
}

void DownloadBubbleUIController::RecordDownloadBubbleInteraction() {
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(
          browser_->profile());
  tracker->NotifyEvent("download_bubble_interaction");
}
