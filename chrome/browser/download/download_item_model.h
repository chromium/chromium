// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_MODEL_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_MODEL_H_

#include <stdint.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_ui_model.h"
#include "components/download/public/common/download_item.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/common/proto/download_file_types.pb.h"

namespace content {
class WebContents;
}  // namespace content

// Implementation of DownloadUIModel that wrappers around a |DownloadItem*|. As
// such, the caller is expected to ensure that the |download| passed into the
// constructor outlives this |DownloadItemModel|. In addition, multiple
// DownloadItemModel objects could be wrapping the same DownloadItem.
class DownloadItemModel : public DownloadUIModel,
                          public download::DownloadItem::Observer {
 public:
#if !BUILDFLAG(IS_ANDROID)
  // How long an ephemeral warning is displayed on the download bubble.
  static constexpr base::TimeDelta kEphemeralWarningLifetimeOnBubble =
      base::Minutes(5);
#endif

  static DownloadUIModelPtr Wrap(download::DownloadItem* download);
  static DownloadUIModelPtr Wrap(
      download::DownloadItem* download,
      std::unique_ptr<DownloadUIModel::StatusTextBuilderBase>
          status_text_builder);

  // Constructs a DownloadItemModel. The caller must ensure that |download|
  // outlives this object.
  explicit DownloadItemModel(download::DownloadItem* download);

  DownloadItemModel(download::DownloadItem* download,
                    std::unique_ptr<DownloadUIModel::StatusTextBuilderBase>
                        status_text_builder);

  DownloadItemModel(const DownloadItemModel&) = delete;
  DownloadItemModel& operator=(const DownloadItemModel&) = delete;

  ~DownloadItemModel() override;

  // DownloadUIModel implementation.
  ContentId GetContentId() const override;
  Profile* profile() const override;
  std::u16string GetTabProgressStatusText() const override;
  int64_t GetCompletedBytes() const override;
  int64_t GetTotalBytes() const override;
  int PercentComplete() const override;
  bool IsDangerous() const override;
  bool MightBeMalicious() const override;
  bool IsMalicious() const override;
  bool IsInsecure() const override;
  bool ShouldRemoveFromShelfWhenComplete() const override;
  bool ShouldShowDownloadStartedAnimation() const override;
  bool ShouldShowInShelf() const override;
  void SetShouldShowInShelf(bool should_show) override;
  bool ShouldNotifyUI() const override;
  bool WasUINotified() const override;
  void SetWasUINotified(bool should_notify) override;
  bool WasActionedOn() const override;
  void SetActionedOn(bool actioned_on) override;
  bool WasUIWarningShown() const override;
  void SetWasUIWarningShown(bool was_ui_warning_shown) override;
  std::optional<base::Time> GetEphemeralWarningUiShownTime() const override;
  void SetEphemeralWarningUiShownTime(std::optional<base::Time> time) override;
  bool ShouldPreferOpeningInBrowser() override;
  void SetShouldPreferOpeningInBrowser(bool preference) override;
  safe_browsing::DownloadFileType::DangerLevel GetDangerLevel() const override;
  void SetDangerLevel(
      safe_browsing::DownloadFileType::DangerLevel danger_level) override;
  download::DownloadItem::InsecureDownloadStatus GetInsecureDownloadStatus()
      const override;
  void OpenUsingPlatformHandler() override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::optional<DownloadCommands::Command> MaybeGetMediaAppAction()
      const override;
  void OpenUsingMediaApp() override;
#endif
  bool IsBeingRevived() const override;
  void SetIsBeingRevived(bool is_being_revived) override;
  const download::DownloadItem* GetDownloadItem() const override;
  base::FilePath GetFileNameToReportUser() const override;
  base::FilePath GetTargetFilePath() const override;
  void OpenDownload() override;
  download::DownloadItem::DownloadState GetState() const override;
  bool IsPaused() const override;
  download::DownloadDangerType GetDangerType() const override;
  bool GetOpenWhenComplete() const override;
  bool IsOpenWhenCompleteByPolicy() const override;
  bool TimeRemaining(base::TimeDelta* remaining) const override;
  base::Time GetStartTime() const override;
  base::Time GetEndTime() const override;
  bool GetOpened() const override;
  void SetOpened(bool opened) override;
  bool IsDone() const override;
  void Pause() override;
  void Cancel(bool user_cancel) override;
  void Resume() override;
  void Remove() override;
  void SetOpenWhenComplete(bool open) override;
  base::FilePath GetFullPath() const override;
  bool CanResume() const override;
  bool AllDataSaved() const override;
  bool GetFileExternallyRemoved() const override;
  GURL GetURL() const override;
  bool HasUserGesture() const override;
  offline_items_collection::FailState GetLastFailState() const override;

#if !BUILDFLAG(IS_ANDROID)
  bool IsCommandEnabled(const DownloadCommands* download_commands,
                        DownloadCommands::Command command) const override;
  bool IsCommandChecked(const DownloadCommands* download_commands,
                        DownloadCommands::Command command) const override;
  void ExecuteCommand(DownloadCommands* download_commands,
                      DownloadCommands::Command command) override;
  TailoredWarningType GetTailoredWarningType() const override;
  DangerUiPattern GetDangerUiPattern() const override;
  bool ShouldShowInBubble() const override;
  bool IsEphemeralWarning() const override;
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
  void CompleteSafeBrowsingScan() override;
  void ReviewScanningVerdict(content::WebContents* web_contents) override;
#endif

  bool ShouldShowDropdown() const override;
  void DetermineAndSetShouldPreferOpeningInBrowser(
      const base::FilePath& target_path,
      bool is_filetype_handled_safely) override;
  bool IsTopLevelEncryptedArchive() const override;
  bool IsExtensionDownload() const override;

  // download::DownloadItem::Observer implementation.
  void OnDownloadUpdated(download::DownloadItem* download) override;
  void OnDownloadOpened(download::DownloadItem* download) override;
  void OnDownloadDestroyed(download::DownloadItem* download) override;

 private:
  // DownloadUIModel implementation.
  std::string GetMimeType() const override;

  // The DownloadItem that this model represents. Note that DownloadItemModel
  // itself shouldn't maintain any state since there can be more than one
  // DownloadItemModel in use with the same DownloadItem.
  raw_ptr<download::DownloadItem> download_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_MODEL_H_
