// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_WARNING_DATA_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_WARNING_DATA_H_

#include <vector>

#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace download {
class DownloadItem;
}

// Per DownloadItem data for storing warning events on download warnings. The
// data is only set if a warning is shown. These events are added to Safe
// Browsing reports.
class DownloadItemWarningData : public base::SupportsUserData::Data {
 public:
  // The surface that the warning is shown. See
  // go/chrome-download-warning-surfaces for details.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class WarningSurface {
    // Applicable actions: DISCARD, OPEN_SUBPAGE
    BUBBLE_MAINPAGE = 1,
    // Applicable actions: PROCEED, DISCARD, DISMISS, CLOSE, BACK,
    // PROCEED_DEEP_SCAN, OPEN_LEARN_MORE_LINK
    BUBBLE_SUBPAGE = 2,
    // Applicable actions: DISCARD, KEEP
    DOWNLOADS_PAGE = 3,
    // Applicable actions: PROCEED, CANCEL, CLOSE
    DOWNLOAD_PROMPT = 4,
    kMaxValue = DOWNLOAD_PROMPT
  };

  // Users action on the warning surface.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class WarningAction {
    // The warning is shown. This is a special action that may not be triggered
    // by user. We will use this action as the anchor to track the latency of
    // other actions.
    SHOWN = 0,
    // The user clicks proceed, which means the user decides to bypass the
    // warning.
    PROCEED = 1,
    // The user clicks discard, which means the user decides to obey the
    // warning and the dangerous download is deleted from disk.
    DISCARD = 2,
    // The user has clicked the keep button on the surface.
    KEEP = 3,
    // The user has clicked the close button on the surface.
    CLOSE = 4,
    // The user clicks cancel on the download prompt.
    CANCEL = 5,
    // The user has dismissed the bubble by clicking anywhere outside
    // the bubble.
    DISMISS = 6,
    // The user has clicked the back button on the bubble subpage to go back
    // to the bubble main page.
    BACK = 7,
    // The user has opened the subpage from the main page.
    OPEN_SUBPAGE = 8,
    // The user clicks proceed on a prompt for deep scanning.
    PROCEED_DEEP_SCAN = 9,
    // The user clicks the learn more link on the bubble subpage.
    OPEN_LEARN_MORE_LINK = 10,
    kMaxValue = OPEN_LEARN_MORE_LINK
  };

  struct WarningActionEvent {
    WarningSurface surface;
    WarningAction action;
    // The latency between when the warning is shown for the first time and when
    // this event has happened.
    int64_t action_latency_msec;
    // A terminal action means that the warning disappears after this event,
    // the download is either deleted or saved.
    bool is_terminal_action = false;

    WarningActionEvent(WarningSurface surface,
                       WarningAction action,
                       int64_t action_latency_msec,
                       bool is_terminal_action);
  };

  ~DownloadItemWarningData() override;

  // Gets all warning actions associated with this `download`. Returns an
  // empty vector if there's no warning data or there is no warning shown for
  // this `download`.
  static std::vector<WarningActionEvent> GetWarningActionEvents(
      const download::DownloadItem* download);

  // Adds an `action` triggered on `surface` for `download`. It may not be
  // added if `download` is null or the length of events associated with this
  // `download` exceeds the limit.
  static void AddWarningActionEvent(download::DownloadItem* download,
                                    WarningSurface surface,
                                    WarningAction action);

  // Returns whether the download was an encrypted archive.
  static bool IsEncryptedArchive(const download::DownloadItem* download);
  static void SetIsEncryptedArchive(download::DownloadItem* download,
                                    bool is_encrypted_archive);

  // Returns whether the user has entered an incorrect password for the
  // archive.
  static bool HasIncorrectPassword(const download::DownloadItem* download);
  static void SetHasIncorrectPassword(download::DownloadItem* download,
                                      bool has_incorrect_password);

  // Converts an `event` to the Safe Browsing report proto format.
  static safe_browsing::ClientSafeBrowsingReportRequest::DownloadWarningAction
  ConstructCsbrrDownloadWarningAction(const WarningActionEvent& event);

  // Returns whether we have shown a local password decryption prompt for this
  // download.
  static bool HasShownLocalDecryptionPrompt(
      const download::DownloadItem* download);
  static void SetHasShownLocalDecryptionPrompt(download::DownloadItem* download,
                                               bool has_shown);

  // Returns whether an encrypted archive was fully extracted.
  static bool IsFullyExtractedArchive(const download::DownloadItem* download);
  static void SetIsFullyExtractedArchive(download::DownloadItem* download,
                                         bool extracted);

 private:
  DownloadItemWarningData();

  template <typename F, typename V>
  static V GetWithDefault(const download::DownloadItem* download,
                          F&& f,
                          V&& default_value);
  static DownloadItemWarningData* GetOrCreate(download::DownloadItem* download);

  std::vector<WarningActionEvent> ActionEvents() const;

  static const char kKey[];

  base::Time warning_first_shown_time_;
  std::vector<WarningActionEvent> action_events_;
  bool is_encrypted_archive_ = false;
  bool has_incorrect_password_ = false;
  bool has_shown_local_decryption_prompt_ = false;
  bool fully_extracted_archive_ = false;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_WARNING_DATA_H_
