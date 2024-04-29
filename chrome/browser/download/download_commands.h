// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_COMMANDS_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_COMMANDS_H_

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
class Browser;
#endif

class DownloadUIModel;

class DownloadCommands {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum Command {
    SHOW_IN_FOLDER = 0,  // Open a folder view window with the item selected.
    OPEN_WHEN_COMPLETE = 1,       // Open the download when it's finished.
    ALWAYS_OPEN_TYPE = 2,         // Default this file extension to always open.
    PLATFORM_OPEN = 3,            // Open using platform handler.
    CANCEL = 4,                   // Cancel the download.
    PAUSE = 5,                    // Pause a download.
    RESUME = 6,                   // Resume a download.
    DISCARD = 7,                  // Discard the malicious download.
    KEEP = 8,                     // Keep the malicious download.
    LEARN_MORE_SCANNING = 9,      // Show info about download scanning.
    LEARN_MORE_INTERRUPTED = 10,  // Show info about interrupted downloads.
    LEARN_MORE_INSECURE_DOWNLOAD = 11,  // Show info about insecure downloads.
    LEARN_MORE_DOWNLOAD_BLOCKED = 12,   // Show info about blocked downloads.
    OPEN_SAFE_BROWSING_SETTING = 13,    // Open settings page for Safe Browsing.
    COPY_TO_CLIPBOARD = 14,             // Copy the contents to the clipboard.
    DEEP_SCAN = 15,             // Send file to Safe Browsing for deep scanning.
    BYPASS_DEEP_SCANNING = 16,  // Bypass the prompt to deep scan.
    REVIEW = 17,                // Show enterprise download review dialog.
    RETRY = 18,                 // Retry the download.
    CANCEL_DEEP_SCAN = 19,  // Cancel deep scan and return to scanning prompt.
    BYPASS_DEEP_SCANNING_AND_OPEN = 20,  // Bypass the prompt to deep scan and
                                         // open the file.
    OPEN_WITH_MEDIA_APP = 21,  // Open file using the ChromeOS media app.
    EDIT_WITH_MEDIA_APP = 22,  // Open file using the ChromeOS media app with
                               // an editing hint.

    kMaxValue = EDIT_WITH_MEDIA_APP,  // Keep last.
  };

  // |model| must outlive DownloadCommands.
  // TODO(shaktisahu): Investigate if model lifetime is shorter than |this|.
  explicit DownloadCommands(base::WeakPtr<DownloadUIModel> model);

  DownloadCommands(const DownloadCommands&) = delete;
  DownloadCommands& operator=(const DownloadCommands&) = delete;

  virtual ~DownloadCommands();

  bool IsCommandEnabled(Command command) const;
  bool IsCommandChecked(Command command) const;
  bool IsCommandVisible(Command command) const;
  void ExecuteCommand(Command command);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
  bool IsDownloadPdf() const;
  bool CanOpenPdfInSystemViewer() const;
  Browser* GetBrowser() const;
#endif

  GURL GetLearnMoreURLForInterruptedDownload() const;
  void CopyFileAsImageToClipboard();
  bool CanBeCopiedToClipboard() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(
      DownloadCommandsTest,
      GetLearnMoreURLForInterruptedDownload_ContainsContext);

  base::WeakPtr<DownloadUIModel> model_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_COMMANDS_H_
