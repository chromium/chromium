// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/offline_item_model.h"

#include <string>

#include "base/observer_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/download/offline_item_model_manager.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/offline_items_collection/core/fail_state.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"

using offline_items_collection::ContentId;
using offline_items_collection::FailState;
using offline_items_collection::OfflineItem;
using offline_items_collection::OfflineItemState;

// static
DownloadUIModel::DownloadUIModelPtr OfflineItemModel::Wrap(
    OfflineItemModelManager* manager,
    const OfflineItem& offline_item) {
  return std::make_unique<OfflineItemModel>(manager, offline_item);
}

// static
DownloadUIModel::DownloadUIModelPtr OfflineItemModel::Wrap(
    OfflineItemModelManager* manager,
    const OfflineItem& offline_item,
    std::unique_ptr<DownloadUIModel::StatusTextBuilderBase>
        status_text_builder) {
  return std::make_unique<OfflineItemModel>(manager, offline_item,
                                            std::move(status_text_builder));
}

OfflineItemModel::OfflineItemModel(OfflineItemModelManager* manager,
                                   const OfflineItem& offline_item)
    : OfflineItemModel(manager,
                       offline_item,
                       std::make_unique<StatusTextBuilder>()) {}

OfflineItemModel::OfflineItemModel(
    OfflineItemModelManager* manager,
    const OfflineItem& offline_item,
    std::unique_ptr<DownloadUIModel::StatusTextBuilderBase> status_text_builder)
    : DownloadUIModel(std::move(status_text_builder)),
      manager_(manager),
      offline_item_(std::make_unique<OfflineItem>(offline_item)) {
  Profile* profile = Profile::FromBrowserContext(manager_->browser_context());
  offline_items_collection::OfflineContentAggregator* aggregator =
      OfflineContentAggregatorFactory::GetForKey(profile->GetProfileKey());
  offline_item_observer_ =
      std::make_unique<FilteredOfflineItemObserver>(aggregator);
  offline_item_observer_->AddObserver(offline_item_->id, this);
}

OfflineItemModel::~OfflineItemModel() {
  if (offline_item_)
    offline_item_observer_->RemoveObserver(offline_item_->id, this);
}

Profile* OfflineItemModel::profile() const {
  return Profile::FromBrowserContext(manager_->browser_context());
}

ContentId OfflineItemModel::GetContentId() const {
  return offline_item_ ? offline_item_->id : ContentId();
}

int64_t OfflineItemModel::GetCompletedBytes() const {
  return offline_item_ ? offline_item_->received_bytes : 0;
}

int64_t OfflineItemModel::GetTotalBytes() const {
  if (!offline_item_)
    return 0;
  return offline_item_->total_size_bytes > 0 ? offline_item_->total_size_bytes
                                             : 0;
}

int OfflineItemModel::PercentComplete() const {
  if (GetTotalBytes() <= 0)
    return -1;

  return static_cast<int>(GetCompletedBytes() * 100.0 / GetTotalBytes());
}

bool OfflineItemModel::WasUINotified() const {
  const OfflineItemModelData* data =
      manager_->GetOrCreateOfflineItemModelData(offline_item_->id);
  return data->was_ui_notified_;
}

void OfflineItemModel::SetWasUINotified(bool was_ui_notified) {
  OfflineItemModelData* data =
      manager_->GetOrCreateOfflineItemModelData(offline_item_->id);
  data->was_ui_notified_ = was_ui_notified;
}

bool OfflineItemModel::WasActionedOn() const {
  const OfflineItemModelData* data =
      manager_->GetOrCreateOfflineItemModelData(offline_item_->id);
  return data->actioned_on_;
}

void OfflineItemModel::SetActionedOn(bool actioned_on) {
  OfflineItemModelData* data =
      manager_->GetOrCreateOfflineItemModelData(offline_item_->id);
  data->actioned_on_ = actioned_on;
}

base::FilePath OfflineItemModel::GetFileNameToReportUser() const {
  return offline_item_ ? base::FilePath::FromUTF8Unsafe(offline_item_->title)
                       : base::FilePath();
}

base::FilePath OfflineItemModel::GetTargetFilePath() const {
  return offline_item_ ? offline_item_->file_path : base::FilePath();
}

void OfflineItemModel::OpenDownload() {
  if (!offline_item_)
    return;

  offline_items_collection::OpenParams open_params(
      offline_items_collection::LaunchLocation::DOWNLOAD_SHELF);
  // TODO(crbug.com/40121163): Determine if we ever need to open in incognito.
  GetProvider()->OpenItem(open_params, offline_item_->id);
}

void OfflineItemModel::Pause() {
  if (!offline_item_)
    return;

  GetProvider()->PauseDownload(offline_item_->id);
}

void OfflineItemModel::Resume() {
  if (!offline_item_)
    return;

  GetProvider()->ResumeDownload(offline_item_->id);
}

void OfflineItemModel::Cancel(bool user_cancel) {
  if (!offline_item_)
    return;

  GetProvider()->CancelDownload(offline_item_->id);
}

void OfflineItemModel::Remove() {
  if (!offline_item_)
    return;

  GetProvider()->RemoveItem(offline_item_->id);
}

download::DownloadItem::DownloadState OfflineItemModel::GetState() const {
  if (!offline_item_)
    return download::DownloadItem::CANCELLED;
  switch (offline_item_->state) {
    case OfflineItemState::IN_PROGRESS:
      [[fallthrough]];
    case OfflineItemState::PAUSED:
      return download::DownloadItem::IN_PROGRESS;
    case OfflineItemState::PENDING:
      [[fallthrough]];
    case OfflineItemState::INTERRUPTED:
      [[fallthrough]];
    case OfflineItemState::FAILED:
      return download::DownloadItem::INTERRUPTED;
    case OfflineItemState::COMPLETE:
      return download::DownloadItem::COMPLETE;
    case OfflineItemState::CANCELLED:
      return download::DownloadItem::CANCELLED;
    case OfflineItemState::NUM_ENTRIES:
      NOTREACHED_IN_MIGRATION();
      return download::DownloadItem::CANCELLED;
  }
}

bool OfflineItemModel::IsPaused() const {
  return offline_item_ ? offline_item_->state == OfflineItemState::PAUSED
                       : true;
}

bool OfflineItemModel::TimeRemaining(base::TimeDelta* remaining) const {
  if (!offline_item_ || offline_item_->time_remaining_ms == -1)
    return false;
  *remaining = base::Milliseconds(offline_item_->time_remaining_ms);
  return true;
}

base::Time OfflineItemModel::GetStartTime() const {
  return offline_item_->creation_time;
}

base::Time OfflineItemModel::GetEndTime() const {
  return offline_item_->completion_time;
}

bool OfflineItemModel::IsDone() const {
  if (!offline_item_)
    return true;
  switch (offline_item_->state) {
    case OfflineItemState::IN_PROGRESS:
      [[fallthrough]];
    case OfflineItemState::PAUSED:
      [[fallthrough]];
    case OfflineItemState::PENDING:
      return false;
    case OfflineItemState::INTERRUPTED:
      return !offline_item_->is_resumable;
    case OfflineItemState::FAILED:
      [[fallthrough]];
    case OfflineItemState::COMPLETE:
      [[fallthrough]];
    case OfflineItemState::CANCELLED:
      return true;
    case OfflineItemState::NUM_ENTRIES:
      NOTREACHED_IN_MIGRATION();
  }
  return false;
}

base::FilePath OfflineItemModel::GetFullPath() const {
  return GetTargetFilePath();
}

bool OfflineItemModel::CanResume() const {
  return offline_item_ ? offline_item_->is_resumable : false;
}

bool OfflineItemModel::AllDataSaved() const {
  return offline_item_ ? offline_item_->state == OfflineItemState::COMPLETE
                       : false;
}

bool OfflineItemModel::GetFileExternallyRemoved() const {
  return offline_item_ ? offline_item_->externally_removed : true;
}

GURL OfflineItemModel::GetURL() const {
  return offline_item_ ? offline_item_->url : GURL();
}

bool OfflineItemModel::ShouldRemoveFromShelfWhenComplete() const {
  // TODO(shaktisahu): Add more appropriate logic.
  return false;
}

OfflineContentProvider* OfflineItemModel::GetProvider() const {
  Profile* profile = Profile::FromBrowserContext(manager_->browser_context());
  offline_items_collection::OfflineContentAggregator* aggregator =
      OfflineContentAggregatorFactory::GetForKey(profile->GetProfileKey());
  return aggregator;
}

void OfflineItemModel::OnItemRemoved(const ContentId& id) {
  offline_item_.reset();
  // The object could get deleted after this.
  if (delegate_)
    delegate_->OnDownloadDestroyed(id);
}

void OfflineItemModel::OnItemUpdated(
    const OfflineItem& item,
    const std::optional<UpdateDelta>& update_delta) {
  offline_item_ = std::make_unique<OfflineItem>(item);
  if (delegate_)
    delegate_->OnDownloadUpdated();
}

FailState OfflineItemModel::GetLastFailState() const {
  return offline_item_ ? offline_item_->fail_state : FailState::USER_CANCELED;
}

GURL OfflineItemModel::GetOriginalURL() const {
  return offline_item_ ? offline_item_->original_url : GURL();
}

bool OfflineItemModel::ShouldPromoteOrigin() const {
  return offline_item_ && offline_item_->promote_origin;
}

#if !BUILDFLAG(IS_ANDROID)
bool OfflineItemModel::IsCommandEnabled(
    const DownloadCommands* download_commands,
    DownloadCommands::Command command) const {
  switch (command) {
    case DownloadCommands::SHOW_IN_FOLDER:
    case DownloadCommands::OPEN_WHEN_COMPLETE:
    case DownloadCommands::PLATFORM_OPEN:
    case DownloadCommands::ALWAYS_OPEN_TYPE:
    case DownloadCommands::OPEN_WITH_MEDIA_APP:
    case DownloadCommands::EDIT_WITH_MEDIA_APP:
      NOTIMPLEMENTED();
      return false;
    case DownloadCommands::PAUSE:
    case DownloadCommands::CANCEL:
    case DownloadCommands::RESUME:
    case DownloadCommands::COPY_TO_CLIPBOARD:
    case DownloadCommands::DISCARD:
    case DownloadCommands::KEEP:
    case DownloadCommands::LEARN_MORE_SCANNING:
    case DownloadCommands::LEARN_MORE_INTERRUPTED:
    case DownloadCommands::LEARN_MORE_INSECURE_DOWNLOAD:
    case DownloadCommands::LEARN_MORE_DOWNLOAD_BLOCKED:
    case DownloadCommands::OPEN_SAFE_BROWSING_SETTING:
    case DownloadCommands::DEEP_SCAN:
    case DownloadCommands::BYPASS_DEEP_SCANNING:
    case DownloadCommands::BYPASS_DEEP_SCANNING_AND_OPEN:
    case DownloadCommands::REVIEW:
    case DownloadCommands::RETRY:
    case DownloadCommands::CANCEL_DEEP_SCAN:
      return DownloadUIModel::IsCommandEnabled(download_commands, command);
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool OfflineItemModel::IsCommandChecked(
    const DownloadCommands* download_commands,
    DownloadCommands::Command command) const {
  switch (command) {
    case DownloadCommands::OPEN_WHEN_COMPLETE:
    case DownloadCommands::ALWAYS_OPEN_TYPE:
      NOTIMPLEMENTED();
      return false;
    case DownloadCommands::PAUSE:
    case DownloadCommands::RESUME:
      return IsPaused();
    case DownloadCommands::SHOW_IN_FOLDER:
    case DownloadCommands::PLATFORM_OPEN:
    case DownloadCommands::CANCEL:
    case DownloadCommands::DISCARD:
    case DownloadCommands::KEEP:
    case DownloadCommands::LEARN_MORE_SCANNING:
    case DownloadCommands::LEARN_MORE_INTERRUPTED:
    case DownloadCommands::LEARN_MORE_INSECURE_DOWNLOAD:
    case DownloadCommands::LEARN_MORE_DOWNLOAD_BLOCKED:
    case DownloadCommands::OPEN_SAFE_BROWSING_SETTING:
    case DownloadCommands::COPY_TO_CLIPBOARD:
    case DownloadCommands::DEEP_SCAN:
    case DownloadCommands::BYPASS_DEEP_SCANNING:
    case DownloadCommands::BYPASS_DEEP_SCANNING_AND_OPEN:
    case DownloadCommands::REVIEW:
    case DownloadCommands::RETRY:
    case DownloadCommands::CANCEL_DEEP_SCAN:
    case DownloadCommands::OPEN_WITH_MEDIA_APP:
    case DownloadCommands::EDIT_WITH_MEDIA_APP:
      return false;
  }
  return false;
}

void OfflineItemModel::ExecuteCommand(DownloadCommands* download_commands,
                                      DownloadCommands::Command command) {
  switch (command) {
    case DownloadCommands::SHOW_IN_FOLDER:
    case DownloadCommands::OPEN_WHEN_COMPLETE:
    case DownloadCommands::ALWAYS_OPEN_TYPE:
    case DownloadCommands::KEEP:
    case DownloadCommands::LEARN_MORE_SCANNING:
    case DownloadCommands::LEARN_MORE_INSECURE_DOWNLOAD:
    case DownloadCommands::LEARN_MORE_DOWNLOAD_BLOCKED:
    case DownloadCommands::OPEN_SAFE_BROWSING_SETTING:
      NOTIMPLEMENTED();
      return;
    case DownloadCommands::PLATFORM_OPEN:
    case DownloadCommands::CANCEL:
    case DownloadCommands::DISCARD:
    case DownloadCommands::LEARN_MORE_INTERRUPTED:
    case DownloadCommands::PAUSE:
    case DownloadCommands::RESUME:
    case DownloadCommands::COPY_TO_CLIPBOARD:
    case DownloadCommands::DEEP_SCAN:
    case DownloadCommands::BYPASS_DEEP_SCANNING:
    case DownloadCommands::BYPASS_DEEP_SCANNING_AND_OPEN:
    case DownloadCommands::REVIEW:
    case DownloadCommands::RETRY:
    case DownloadCommands::CANCEL_DEEP_SCAN:
    case DownloadCommands::OPEN_WITH_MEDIA_APP:
    case DownloadCommands::EDIT_WITH_MEDIA_APP:
      DownloadUIModel::ExecuteCommand(download_commands, command);
      break;
  }
}
#endif

std::string OfflineItemModel::GetMimeType() const {
  return offline_item_ ? offline_item_->mime_type : "";
}
