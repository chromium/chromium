// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UI_MODEL_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UI_MODEL_H_

#include <stdint.h>

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "components/download/public/common/download_item.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/common/proto/download_file_types.pb.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/vector_icon_types.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/download/download_commands.h"
#endif

using offline_items_collection::ContentId;

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class FontList;
}  // namespace gfx

// This class is an abstraction for common UI tasks and properties associated
// with a download.
class DownloadUIModel {
 public:
  // The type of tailored warning that is shown.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class TailoredWarningType {
    kNoTailoredWarning = 0,
    // Base cookie theft warning.
    kCookieTheft = 1,
    // Cookie theft warning with account info.
    kCookieTheftWithAccountInfo = 2,
    // Suspicious archive warning.
    kSuspiciousArchive = 3,
    kMaxValue = kSuspiciousArchive
  };

  // Represents the UI pattern used for the download, based on its danger type.
  // This should be consistent across the download bubble and
  // chrome://downloads, wherever download warnings are displayed.
  enum class DangerUiPattern {
    // The download has no warning and no error conditions.
    kNormal,
    // The download has a warning with a red "dangerous" icon and red text.
    kDangerous,
    // The download has a warning with a gray "warning" icon and gray text.
    // Includes "unverified" and insecure.
    kSuspicious,
    // Some other combination of warning colors/icons, including the "download
    // off" icon for an error or cancellation.
    kOther,
  };

  // Abstract base class for building StatusText
  class StatusTextBuilderBase {
   public:
    virtual ~StatusTextBuilderBase() = default;
    void SetModel(DownloadUIModel* model);

    // Returns a short one-line status string for the download.
    std::u16string GetStatusText(
        download::DownloadItem::DownloadState state) const;

    std::u16string GetCompletedRemovedOrSavedStatusText() const;
    // Returns a string indicating the status of an in-progress download.
    virtual std::u16string GetInProgressStatusText() const = 0;

    // Returns a string indicating the status of a completed download.
    virtual std::u16string GetCompletedStatusText() const = 0;

    // Returns a string representation of the current download progress sizes.
    // If the total size of the download is known, this string looks like:
    // "100/200 MB" where the numerator is the transferred size and the
    // denominator is the total size. If the total isn't known, returns the
    // transferred size as a string (e.g.: "100 MB").
    virtual std::u16string GetProgressSizesString() const = 0;

    // Returns a string indicating the status of an interrupted download.
    virtual std::u16string GetInterruptedStatusText(
        offline_items_collection::FailState fail_state) const;

    // Returns a short string indicating why the download failed.
    virtual std::u16string GetFailStateMessage(
        offline_items_collection::FailState fail_state) const;

    // Unknowned model to create statuses.
    raw_ptr<DownloadUIModel> model_ = nullptr;
  };

  // Used in Download shelf and page, default option.
  class StatusTextBuilder : public StatusTextBuilderBase {
   public:
    std::u16string GetInProgressStatusText() const override;
    std::u16string GetCompletedStatusText() const override;
    std::u16string GetProgressSizesString() const override;
  };

  // Used in Download bubble.
  class BubbleStatusTextBuilder : public StatusTextBuilderBase {
   public:
    std::u16string GetInProgressStatusText() const override;
    std::u16string GetCompletedStatusText() const override;
    std::u16string GetInterruptedStatusText(
        offline_items_collection::FailState fail_state) const override;
    std::u16string GetProgressSizesString() const override;

   private:
    FRIEND_TEST_ALL_PREFIXES(DownloadItemModelTest,
                             GetBubbleStatusMessageWithBytes);

    static std::u16string GetBubbleStatusMessageWithBytes(
        const std::u16string& bytes_substring,
        const std::u16string& detail_message,
        bool is_active);
    std::u16string GetBubbleWarningStatusText() const;
  };

  using DownloadUIModelPtr = std::unique_ptr<DownloadUIModel>;

  DownloadUIModel();

  explicit DownloadUIModel(
      std::unique_ptr<StatusTextBuilderBase> status_text_builder);

  DownloadUIModel(const DownloadUIModel&) = delete;
  DownloadUIModel& operator=(const DownloadUIModel&) = delete;

  virtual ~DownloadUIModel();

  // Delegate for a single DownloadUIModel.
  class Delegate {
   public:
    virtual void OnDownloadUpdated() {}
    virtual void OnDownloadOpened() {}
    virtual void OnDownloadDestroyed(const ContentId& id) {}

    virtual ~Delegate() = default;
  };

  void SetDelegate(Delegate* delegate);

  base::WeakPtr<DownloadUIModel> GetWeakPtr();

  // Does this download have a MIME type (either explicit or inferred from its
  // extension) suggesting that it is a supported image type?
  bool HasSupportedImageMimeType() const;

  // Returns a string representation of the current download progress sizes.
  // If the total size of the download is known, this string looks like:
  // "100/200 MB" where the numerator is the transferred size and the
  // denominator is the total size. If the total isn't known, returns the
  // transferred size as a string (e.g.: "100 MB").
  std::u16string GetProgressSizesString() const;

  // Returns a long descriptive string for a download that's in the INTERRUPTED
  // state. For other downloads, the returned string will be empty.
  std::u16string GetInterruptDescription() const;

  // Returns a status string for the download history page.
  std::u16string GetHistoryPageStatusText() const;

  // Returns a short one-line status string for the download.
  std::u16string GetStatusText() const;
#if !BUILDFLAG(IS_ANDROID)
  std::u16string GetStatusTextForLabel(const gfx::FontList& font_list,
                                       float available_pixel_width) const;
#endif

  // Returns a string suitable for use as a tooltip. For a regular download, the
  // tooltip is the filename. For an interrupted download, the string states the
  // filename and a short description of the reason for interruption. For
  // example:
  //    Report.pdf
  //    Network disconnected
  std::u16string GetTooltipText() const;

  // Get the warning text to display for a dangerous download. |filename| is the
  // (possibly-elided) filename; if it is present in the resulting string,
  // |offset| will be set to the starting position of the filename.
  std::u16string GetWarningText(const std::u16string& filename,
                                size_t* offset) const;

  // Get the caption text for a button for confirming a dangerous download
  // warning.
  std::u16string GetWarningConfirmButtonText() const;

  // Get the text to display for the button to show item in folder on download
  // history page.
  std::u16string GetShowInFolderText() const;

  // Returns the profile associated with this download.
  virtual Profile* profile() const;

  // Returns the content id associated with this download.
  virtual ContentId GetContentId() const;

  // Returns the localized status text for an in-progress download. This
  // is the progress status used in the WebUI interface.
  virtual std::u16string GetTabProgressStatusText() const;

  // Get the number of bytes that has completed so far.
  virtual int64_t GetCompletedBytes() const;

  // Get the total number of bytes for this download. Should return 0 if the
  // total size of the download is not known. Virtual for testing.
  virtual int64_t GetTotalBytes() const;

  // Rough percent complete. Returns -1 if the progress is unknown.
  virtual int PercentComplete() const;

  // Is this considered a dangerous download?
  virtual bool IsDangerous() const;

  // Is this considered a malicious download? Implies IsDangerous().
  virtual bool MightBeMalicious() const;

  // Is this considered a malicious download with very high confidence?
  // Implies IsDangerous() and MightBeMalicious().
  virtual bool IsMalicious() const;

  // Is this download an insecure download, but not something more severe?
  // Implies IsDangerous() and !IsMalicious().
  virtual bool IsInsecure() const;

  // Returns |true| if this download is expected to complete successfully and
  // thereafter be removed from the shelf.  Downloads that are opened
  // automatically or are temporary will be removed from the shelf on successful
  // completion.
  //
  // Returns |false| if the download is not expected to complete (interrupted,
  // cancelled, dangerous, malicious), or won't be removed on completion.
  //
  // Since the expectation of successful completion may change, the return value
  // of this function will change over the course of a download.
  virtual bool ShouldRemoveFromShelfWhenComplete() const;

  // Returns |true| if the download started animation (big download arrow
  // animates down towards the shelf) should be displayed for this download.
  // Downloads that were initiated via "Save As" or are extension installs don't
  // show the animation.
  virtual bool ShouldShowDownloadStartedAnimation() const;

  // Returns |true| if this download should be displayed in the downloads shelf.
  virtual bool ShouldShowInShelf() const;

  // Change whether the download should be displayed on the downloads
  // shelf. Setting this is only effective if the download hasn't already been
  // displayed in the shelf.
  virtual void SetShouldShowInShelf(bool should_show);

  // Returns |true| if the UI should be notified when the download is ready to
  // be presented in the UI. Note that this is independent of
  // ShouldShowInShelf() since there might be actions other than showing in the
  // shelf that the UI must perform.
  virtual bool ShouldNotifyUI() const;

  // Returns |true| if the UI has been notified about this download. By default,
  // this value is |false| and should be changed explicitly using
  // SetWasUINotified().
  virtual bool WasUINotified() const;

  // Change what's returned by WasUINotified().
  virtual void SetWasUINotified(bool should_notify);

  // Returns |true| if the download was actioned on. This governs if the
  // download should be shown in the Download Bubble's partial view.
  virtual bool WasActionedOn() const;

  // Change what's returned by WasActionedOn().
  virtual void SetActionedOn(bool actioned_on);

  // Returns |true| if the UI (download bubble, downloads page, notification)
  // has shown this download warning. By default, this value is |false| and
  // should be changed explicitly using SetWasUIWarningShown(). Used to prevent
  // double-logging of download warnings.
  virtual bool WasUIWarningShown() const;

  // Change what's returned by WasUIWarningShown().
  virtual void SetWasUIWarningShown(bool was_ui_warning_shown);

  // If this is an ephemeral warning, returns when the bubble first displayed
  // the warning. If the warning has not yet shown (or this isn't an ephemeral
  // warning), it returns no value. This does not persist across restarts.
  virtual std::optional<base::Time> GetEphemeralWarningUiShownTime() const;

  virtual void SetEphemeralWarningUiShownTime(std::optional<base::Time> time);

  // Returns |true| if opening in the browser is preferred for this download. If
  // |false|, the download should be opened with the system default application.
  virtual bool ShouldPreferOpeningInBrowser();

  // Change what's returned by ShouldPreferOpeningInBrowser to |preference|.
  virtual void SetShouldPreferOpeningInBrowser(bool preference);

  // Return the danger level determined during download target determination.
  // The value returned here is independent of the danger level as determined by
  // the Safe Browsing.
  virtual safe_browsing::DownloadFileType::DangerLevel GetDangerLevel() const;

  // Change what's returned by GetDangerLevel().
  virtual void SetDangerLevel(
      safe_browsing::DownloadFileType::DangerLevel danger_level);

  // Return the mixed content status determined during download target
  // determination.
  virtual download::DownloadItem::InsecureDownloadStatus
  GetInsecureDownloadStatus() const;

  // Open the download using the platform handler for the download. The behavior
  // of this method will be different from DownloadItem::OpenDownload() if
  // ShouldPreferOpeningInBrowser().
  virtual void OpenUsingPlatformHandler();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Returns the Media App action (open or edit) we should show for the item if
  // one should be shown.
  virtual std::optional<DownloadCommands::Command> MaybeGetMediaAppAction()
      const;

  // Open the download using the media app ('Gallery').
  virtual void OpenUsingMediaApp();
#endif

  // Whether the download was removed and this is currently being undone.
  virtual bool IsBeingRevived() const;

  // Set whether the download is being revived.
  virtual void SetIsBeingRevived(bool is_being_revived);

  // Returns the DownloadItem if this is a regular download, or nullptr
  // otherwise.
  virtual const download::DownloadItem* GetDownloadItem() const;
  download::DownloadItem* GetDownloadItem();

  // Returns the file-name that should be reported to the user.
  virtual base::FilePath GetFileNameToReportUser() const;

  // Target path of an in-progress download.
  // May be empty if the target path hasn't yet been determined.
  virtual base::FilePath GetTargetFilePath() const;

  // Opens the file associated with this download.  If the download is
  // still in progress, marks the download to be opened when it is complete.
  virtual void OpenDownload();

  // Returns the current state of the download.
  virtual download::DownloadItem::DownloadState GetState() const;

  // Returns whether the download is currently paused.
  virtual bool IsPaused() const;

  // Returns the danger type associated with this download.
  virtual download::DownloadDangerType GetDangerType() const;

  // Returns true if the download will be auto-opened when complete.
  virtual bool GetOpenWhenComplete() const;

  // Returns true if the download will be auto-opened when complete by policy.
  virtual bool IsOpenWhenCompleteByPolicy() const;

  // Simple calculation of the amount of time remaining to completion. Fills
  // |*remaining| with the amount of time remaining if successful. Fails and
  // returns false if we do not have the number of bytes or the download speed,
  // and so can't give an estimate.
  virtual bool TimeRemaining(base::TimeDelta* remaining) const;

  // Returns the creation time for a download.
  virtual base::Time GetStartTime() const;

  // Returns the end/completion time for a completed download. base::Time()
  // if the download has not completed yet.
  virtual base::Time GetEndTime() const;

  // Returns true if the download has been opened.
  virtual bool GetOpened() const;

  // Marks the download as having been opened (without actually opening it).
  virtual void SetOpened(bool opened);

  // Returns true if the download is in a terminal state. This includes
  // completed downloads, cancelled downloads, and interrupted downloads that
  // can't be resumed.
  virtual bool IsDone() const;

  // Pauses a download.  Will have no effect if the download is already
  // paused.
  virtual void Pause();

  // Resumes a download that has been paused or interrupted. Will have no effect
  // if the download is neither. Only does something if CanResume() returns
  // true.
  virtual void Resume();

  // Cancels the download operation. Set |user_cancel| to true if the
  // cancellation was triggered by an explicit user action.
  virtual void Cancel(bool user_cancel);

  // Removes the download from the views and history. If the download was
  // in-progress or interrupted, then the intermediate file will also be
  // deleted.
  virtual void Remove();

  // Marks the download to be auto-opened when completed.
  virtual void SetOpenWhenComplete(bool open);

  // Returns the full path to the downloaded or downloading file. This is the
  // path to the physical file, if one exists.
  virtual base::FilePath GetFullPath() const;

  // Returns whether the download can be resumed.
  virtual bool CanResume() const;

  // Returns whether this download has saved all of its data.
  virtual bool AllDataSaved() const;

  // Returns whether the file associated with the download has been removed by
  // external action.
  virtual bool GetFileExternallyRemoved() const;

  // Returns the URL represented by this download.
  virtual GURL GetURL() const;

  // Returns whether the download request was initiated in response to a user
  // gesture.
  virtual bool HasUserGesture() const;

  // Returns the most recent failure reason for this download. Returns
  // |FailState::NO_FAILURE| if there is no previous failure reason.
  virtual offline_items_collection::FailState GetLastFailState() const;

  // Returns the URL of the orginiating request.
  virtual GURL GetOriginalURL() const;

  // Whether the Origin should be clearly displayed in the notification for
  // security reasons.
  virtual bool ShouldPromoteOrigin() const;

#if !BUILDFLAG(IS_ANDROID)
  // Methods related to DownloadCommands.
  // Returns whether the given download command is enabled for this download.
  virtual bool IsCommandEnabled(const DownloadCommands* download_commands,
                                DownloadCommands::Command command) const;

  // Returns whether the given download command is checked for this download.
  virtual bool IsCommandChecked(const DownloadCommands* download_commands,
                                DownloadCommands::Command command) const;

  // Executes the given download command on this download.
  virtual void ExecuteCommand(DownloadCommands* download_commands,
                              DownloadCommands::Command command);

  // Returns |true| if this download should be displayed in the download bubble.
  // Note that this may return true even if the download bubble is not enabled
  // on the platform.
  virtual bool ShouldShowInBubble() const;

  // Returns the type of tailored warning. Returns kNoTailoredWarning if this
  // download shouldn't trigger a tailored warning.
  virtual TailoredWarningType GetTailoredWarningType() const;

  // Returns the UI pattern to be used for the download, e.g. dangerous or
  // suspicious. Returns kNoWarning if the download has no warning.
  virtual DangerUiPattern GetDangerUiPattern() const;

  // Ephemeral warnings are ones that are quickly removed from the bubble if the
  // user has not acted on them, and later deleted altogether. Is this that kind
  // of warning?
  virtual bool IsEphemeralWarning() const;
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
  // Complete the Safe Browsing scan early.
  virtual void CompleteSafeBrowsingScan();

  // Open a dialog to review a scan verdict.
  virtual void ReviewScanningVerdict(content::WebContents* web_contents);
#endif

  // Whether the dropdown menu button should be shown or not.
  virtual bool ShouldShowDropdown() const;

  // Determines if a download should be preferably opened in the browser instead
  // of the platform. Use |is_filetype_handled_safely| indicating if opening a
  // file of this type is safe in the current BrowserContext, |target_path| to
  // see if files of this type should be opened in the browser, and set whether
  // the download should be preferred opening in the browser.
  virtual void DetermineAndSetShouldPreferOpeningInBrowser(
      const base::FilePath& target_path,
      bool is_filetype_handled_safely);

  // Returns the accessible alert text that should be announced when the
  // download is in progress.
  virtual std::u16string GetInProgressAccessibleAlertText() const;

  // Determines whether the file is an encrypted archive at the top
  // level (i.e. the encryption is not within a nested archive). This is
  // used to specialize certain strings.
  virtual bool IsTopLevelEncryptedArchive() const;

  // Returns whether the download is triggered by an extension.
  virtual bool IsExtensionDownload() const;

 protected:
  // Returns the MIME type of the download.
  virtual std::string GetMimeType() const;

  raw_ptr<Delegate> delegate_ = nullptr;

 private:
  friend class DownloadItemModelTest;

  void set_clock_for_testing(base::Clock* clock);

  void set_status_text_builder_for_testing(bool for_bubble);

  // Unowned Clock to override the time of "Now".
  raw_ptr<base::Clock> clock_ = base::DefaultClock::GetInstance();

  std::unique_ptr<StatusTextBuilderBase> status_text_builder_;

  base::WeakPtrFactory<DownloadUIModel> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UI_MODEL_H_
