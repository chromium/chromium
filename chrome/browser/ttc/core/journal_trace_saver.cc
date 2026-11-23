// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/journal_trace_saver.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ttc/core/session_journal_impl.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/select_file_policy/chrome_select_file_policy.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "ui/base/base_window.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace ttc {

namespace {

void WriteTraceToFile(const base::FilePath& path,
                      std::vector<uint8_t> trace_data) {
  if (!base::WriteFile(path, trace_data)) {
    LOG(ERROR) << "Failed to write TTC trace to " << path;
  }
}

}  // namespace

JournalTraceSaver::JournalTraceSaver() = default;

JournalTraceSaver::~JournalTraceSaver() {
  if (select_file_dialog_) {
    select_file_dialog_->ListenerDestroyed();
  }
}

void JournalTraceSaver::SaveDebugTrace(content::WebContents* web_contents,
                                       BrowserWindowInterface* browser,
                                       SessionJournalImpl& journal) {
  if (select_file_dialog_ || !journal.HasInMemorySerializer()) {
    return;
  }

  CHECK(SessionJournalImpl::IsInMemoryJournalEnabled());
  pending_trace_data_ = journal.GetSnapshot();
  CHECK(!pending_trace_data_.empty());

  base::FilePath default_dir;
  base::PathService::Get(chrome::DIR_USER_DOCUMENTS, &default_dir);

  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  base::FilePath default_file = default_dir.AppendASCII(
      absl::StrFormat("ttc_trace_%04d_%02d_%02d_%02d_%02d_%02d.pftrace",
                      exploded.year, exploded.month, exploded.day_of_month,
                      exploded.hour, exploded.minute, exploded.second));

  gfx::NativeWindow owning_window = gfx::NativeWindow();
  if (browser && browser->GetWindow()) {
    owning_window = browser->GetWindow()->GetNativeWindow();
  } else if (web_contents) {
    owning_window = web_contents->GetTopLevelNativeWindow();
  }

  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions = {{FILE_PATH_LITERAL("pftrace")}};

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, std::u16string(), default_file,
      &file_type_info, 1, FILE_PATH_LITERAL("pftrace"), owning_window);
}

void JournalTraceSaver::FileSelected(const ui::SelectedFileInfo& file,
                                     int index) {
  std::vector<uint8_t> trace_data = std::move(pending_trace_data_);
  pending_trace_data_.clear();
  select_file_dialog_.reset();

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&WriteTraceToFile, file.path(), std::move(trace_data)));
}

void JournalTraceSaver::FileSelectionCanceled() {
  pending_trace_data_.clear();
  select_file_dialog_.reset();
}

}  // namespace ttc
