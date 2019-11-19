// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_COMMANDS_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_COMMANDS_H_

#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/public/browser/page_navigator.h"
#include "ui/gfx/image/image.h"

#if !defined(OS_ANDROID)
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
    COPY_TO_CLIPBOARD,    // Copy the contents to the clipboard.
    ANNOTATE,             // Open an app to annotate the image.
  };

  // |model| must outlive DownloadCommands.
  // TODO(shaktisahu): Investigate if model lifetime is shorter than |this|.
  explicit DownloadCommands(DownloadUIModel* model);
  virtual ~DownloadCommands();

  bool IsCommandEnabled(Command command) const;
  bool IsCommandChecked(Command command) const;
  bool IsCommandVisible(Command command) const;
  void ExecuteCommand(Command command);

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
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

  DownloadUIModel* model_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(DownloadCommands);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_COMMANDS_H_
