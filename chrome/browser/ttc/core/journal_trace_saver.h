// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_JOURNAL_TRACE_SAVER_H_
#define CHROME_BROWSER_TTC_CORE_JOURNAL_TRACE_SAVER_H_

#include <stdint.h>

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}

namespace ttc {

class SessionJournalImpl;

// Prompts the user for a file path and writes a snapshot of a session's
// in-memory actor journal trace to disk for developer/dogfood debugging. Owned
// by `TtcKeyedService` so that an open file picker survives if the session ends
// before the user finishes choosing a destination.
class JournalTraceSaver : public ui::SelectFileDialog::Listener {
 public:
  JournalTraceSaver();
  JournalTraceSaver(const JournalTraceSaver&) = delete;
  JournalTraceSaver& operator=(const JournalTraceSaver&) = delete;
  ~JournalTraceSaver() override;

  void SaveDebugTrace(content::WebContents* web_contents,
                      BrowserWindowInterface* browser,
                      SessionJournalImpl& journal);

  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

 private:
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  std::vector<uint8_t> pending_trace_data_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_JOURNAL_TRACE_SAVER_H_
