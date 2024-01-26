// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_H_
#define CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_ui_model.h"
#include "components/offline_items_collection/core/filtered_offline_item_observer.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "components/offline_items_collection/core/offline_item.h"

class OfflineItemModelManager;

using offline_items_collection::ContentId;
using offline_items_collection::FilteredOfflineItemObserver;
using offline_items_collection::OfflineContentProvider;
using offline_items_collection::OfflineItem;
using offline_items_collection::UpdateDelta;

// Implementation of DownloadUIModel that wrappers around a |OfflineItem|.
class OfflineItemModel : public DownloadUIModel,
                         public FilteredOfflineItemObserver::Observer {
 public:
  static DownloadUIModelPtr Wrap(OfflineItemModelManager* manager,
                                 const OfflineItem& offline_item);
  static DownloadUIModelPtr Wrap(
      OfflineItemModelManager* manager,
      const OfflineItem& offline_item,
      std::unique_ptr<DownloadUIModel::StatusTextBuilderBase>
          status_text_builder);

  // Constructs a OfflineItemModel.
  OfflineItemModel(OfflineItemModelManager* manager,
                   const OfflineItem& offline_item);
  OfflineItemModel(OfflineItemModelManager* manager,
                   const OfflineItem& offline_item,
                   std::unique_ptr<DownloadUIModel::StatusTextBuilderBase>
                       status_text_builder);

  OfflineItemModel(const OfflineItemModel&) = delete;
  OfflineItemModel& operator=(const OfflineItemModel&) = delete;

  ~OfflineItemModel() override;

  // DownloadUIModel implementation.
  Profile* profile() const override;
  ContentId GetContentId() const override;
  int64_t GetCompletedBytes() const override;
  int64_t GetTotalBytes() const override;
  int PercentComplete() const override;
  bool WasUINotified() const override;
  void SetWasUINotified(bool should_notify) override;
  bool WasActionedOn() const override;
  void SetActionedOn(bool actioned_on) override;
  base::FilePath GetFileNameToReportUser() const override;
  base::FilePath GetTargetFilePath() const override;
  void OpenDownload() override;
  void Pause() override;
  void Resume() override;
  void Cancel(bool user_cancel) override;
  void Remove() override;
  download::DownloadItem::DownloadState GetState() const override;
  bool IsPaused() const override;
  bool TimeRemaining(base::TimeDelta* remaining) const override;
  base::Time GetStartTime() const override;
  base::Time GetEndTime() const override;
  bool IsDone() const override;
  base::FilePath GetFullPath() const override;
  bool CanResume() const override;
  bool AllDataSaved() const override;
  bool GetFileExternallyRemoved() const override;
  GURL GetURL() const override;
  bool ShouldRemoveFromShelfWhenComplete() const override;
  offline_items_collection::FailState GetLastFailState() const override;
  GURL GetOriginalURL() const override;
  bool ShouldPromoteOrigin() const override;

#if !BUILDFLAG(IS_ANDROID)
  bool IsCommandEnabled(const DownloadCommands* download_commands,
                        DownloadCommands::Command command) const override;
  bool IsCommandChecked(const DownloadCommands* download_commands,
                        DownloadCommands::Command command) const override;
  void ExecuteCommand(DownloadCommands* download_commands,
                      DownloadCommands::Command command) override;
#endif

 private:
  OfflineContentProvider* GetProvider() const;

  // FilteredOfflineItemObserver::Observer overrides.
  void OnItemRemoved(const ContentId& id) override;
  void OnItemUpdated(const OfflineItem& item,
                     const std::optional<UpdateDelta>& update_delta) override;

  // DownloadUIModel implementation.
  std::string GetMimeType() const override;

  raw_ptr<OfflineItemModelManager> manager_;

  std::unique_ptr<FilteredOfflineItemObserver> offline_item_observer_;
  std::unique_ptr<OfflineItem> offline_item_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_H_
