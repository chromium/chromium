// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_H_
#define CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_H_

#include <memory>

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

  // Constructs a OfflineItemModel.
  OfflineItemModel(OfflineItemModelManager* manager,
                   const OfflineItem& offline_item);
  ~OfflineItemModel() override;

  // DownloadUIModel implementation.
  Profile* profile() const override;
  ContentId GetContentId() const override;
  int64_t GetCompletedBytes() const override;
  int64_t GetTotalBytes() const override;
  int PercentComplete() const override;
  bool WasUINotified() const override;
  void SetWasUINotified(bool should_notify) override;
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

#if !defined(OS_ANDROID)
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
                     const base::Optional<UpdateDelta>& update_delta) override;

  // DownloadUIModel implementation.
  std::string GetMimeType() const override;

  OfflineItemModelManager* manager_;

  std::unique_ptr<FilteredOfflineItemObserver> offline_item_observer_;
  std::unique_ptr<OfflineItem> offline_item_;

  DISALLOW_COPY_AND_ASSIGN(OfflineItemModel);
};

#endif  // CHROME_BROWSER_DOWNLOAD_OFFLINE_ITEM_MODEL_H_
