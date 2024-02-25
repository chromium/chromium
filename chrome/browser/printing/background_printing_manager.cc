// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/background_printing_manager.h"

#include "base/containers/contains.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

using content::BrowserContext;
using content::BrowserThread;
using content::WebContents;

namespace printing {

class BackgroundPrintingManager::Observer
    : public content::WebContentsObserver {
 public:
  Observer(BackgroundPrintingManager* manager, WebContents* web_contents);

 private:
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

  raw_ptr<BackgroundPrintingManager> manager_;
};

BackgroundPrintingManager::Observer::Observer(
    BackgroundPrintingManager* manager, WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      manager_(manager) {
}

void BackgroundPrintingManager::Observer::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  manager_->DeletePreviewContents(web_contents());
}

BackgroundPrintingManager::BackgroundPrintingManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

BackgroundPrintingManager::~BackgroundPrintingManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The might be some WebContentses still in `printing_contents_map_` at this
  // point (e.g. when the last remaining tab closes and there is still a print
  // preview WebContents trying to print). In such a case it will fail to print,
  // but we should at least clean up the observers.
  // TODO(thestig): Handle this case better.
}

void BackgroundPrintingManager::OwnPrintPreviewDialog(
    std::unique_ptr<WebContents> preview_dialog) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(PrintPreviewDialogController::IsPrintPreviewURL(
      preview_dialog->GetVisibleURL()));
  CHECK(!HasPrintPreviewDialog(preview_dialog.get()));

  WebContents* raw_preview_dialog = preview_dialog.get();
  PrintingContents printing_contents;
  printing_contents.observer =
      std::make_unique<Observer>(this, raw_preview_dialog);
  printing_contents.contents = std::move(preview_dialog);
  printing_contents_map_[raw_preview_dialog] = std::move(printing_contents);
}

void BackgroundPrintingManager::DeletePreviewContentsForBrowserContext(
    BrowserContext* browser_context) {
  std::vector<WebContents*> preview_contents_to_delete;
  for (const auto& iter : printing_contents_map_) {
    WebContents* preview_contents = iter.first;
    if (preview_contents->GetBrowserContext() == browser_context) {
      preview_contents_to_delete.push_back(preview_contents);
    }
  }

  for (size_t i = 0; i < preview_contents_to_delete.size(); i++) {
    DeletePreviewContents(preview_contents_to_delete[i]);
  }
}

void BackgroundPrintingManager::OnPrintRequestCancelled(
    WebContents* preview_contents) {
  DeletePreviewContents(preview_contents);
}

void BackgroundPrintingManager::DeletePreviewContents(
    WebContents* preview_contents) {
  auto i = printing_contents_map_.find(preview_contents);
  if (i == printing_contents_map_.end()) {
    // Everyone is racing to be the first to delete the `preview_contents`. If
    // this case is hit, someone else won the race, so there is no need to
    // continue. <http://crbug.com/100806>
    return;
  }

  std::unique_ptr<WebContents> contents_to_delete =
      std::move(i->second.contents);
  printing_contents_map_.erase(i);

  // ... and mortally wound the contents. Deletion immediately is not a good
  // idea in case this was triggered by `preview_contents` far up the
  // callstack. (Trace where the NOTIFICATION_PRINT_JOB_RELEASED comes from.)
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(contents_to_delete));
}

std::set<content::WebContents*> BackgroundPrintingManager::CurrentContentSet() {
  std::set<content::WebContents*> result;
  for (const auto& entry : printing_contents_map_)
    result.insert(entry.first);

  return result;
}

bool BackgroundPrintingManager::HasPrintPreviewDialog(
    WebContents* preview_dialog) {
  return base::Contains(printing_contents_map_, preview_dialog);
}

BackgroundPrintingManager::PrintingContents::PrintingContents() = default;
BackgroundPrintingManager::PrintingContents::~PrintingContents() = default;
BackgroundPrintingManager::PrintingContents::PrintingContents(
    PrintingContents&&) = default;
BackgroundPrintingManager::PrintingContents&
BackgroundPrintingManager::PrintingContents::operator=(PrintingContents&&) =
    default;

}  // namespace printing
