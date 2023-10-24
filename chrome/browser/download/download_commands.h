// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_COMMANDS_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_COMMANDS_H_

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "content/public/browser/page_navigator.h"
#include "ui/gfx/image/image.h"

#if !BUILDFLAG(IS_ANDROID)
class Browser;
#endif

class DownloadUIModel;

class DownloadCommands {
 public:
  enum Command {
    SHOW_IN_FOLDER = 1,   // Open a folder view window with the item selected.
    OPEN_WHEN_COMPLETE,   // Open the download when it's finished.
    ALWAYS_OPEN_TYPE,     // Default this file extension to always open.
    PLATFORM_OPEN,        // Open using platform handler.
    CANCEL,               // Cancel the download.
    PAUSE,                // Pause a download.
    RESUME,               // Resume a download.
    DISCARD,              // Discard the malicious download.
    KEEP,                 // Keep the malicious download.
    LEARN_MORE_SCANNING,  // Show information about download scanning.
    LEARN_MORE_INTERRUPTED,  // Show information about interrupted downloads.
    LEARN_MORE_INSECURE_DOWNLOAD,  // Show info about insecure downloads.
    LEARN_MORE_DOWNLOAD_BLOCKED,   // Show info about blocked downloads.
    OPEN_SAFE_BROWSING_SETTING,    // Open the settings page for Safe Browsing.
    COPY_TO_CLIPBOARD,             // Copy the contents to the clipboard.
    DEEP_SCAN,             // Send file to Safe Browsing for deep scanning.
    BYPASS_DEEP_SCANNING,  // Bypass the prompt to deep scan.
    REVIEW,                // Show enterprise download review dialog.
    RETRY,                 // Retry the download.
    CANCEL_DEEP_SCAN,      // Cancel the deep scan, returning to a prompt for
                           // scanning.
    BYPASS_DEEP_SCANNING_AND_OPEN = 21,  // Bypass the prompt to deep scan and
                                         // open the file.
    MAX
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
    BUILDFLAG(IS_MAC) || BUILDFLAG(IS_FUCHSIA)
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
