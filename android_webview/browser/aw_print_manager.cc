// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_print_manager.h"

#include <utility>

#include "base/file_descriptor_posix.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print.mojom.h"
#include "components/printing/common/print_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"

namespace android_webview {

namespace {

uint32_t SaveDataToFd(int fd,
                      uint32_t page_count,
                      scoped_refptr<base::RefCountedSharedMemoryMapping> data) {
  bool result = fd > base::kInvalidFd &&
                base::IsValueInRangeForNumericType<int>(data->size());
  if (result)
    result = base::WriteFileDescriptor(fd, *data);
  return result ? page_count : 0;
}

}  // namespace

AwPrintManager::AwPrintManager(content::WebContents* contents)
    : PrintManager(contents),
      content::WebContentsUserData<AwPrintManager>(*contents) {}

AwPrintManager::~AwPrintManager() = default;

// static
void AwPrintManager::BindPrintManagerHost(
    mojo::PendingAssociatedReceiver<printing::mojom::PrintManagerHost> receiver,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* print_manager = AwPrintManager::FromWebContents(web_contents);
  if (!print_manager)
    return;
  print_manager->BindReceiver(std::move(receiver), rfh);
}

void AwPrintManager::PdfWritingDone(int page_count) {
  pdf_writing_done_callback().Run(page_count);
  // Invalidate the file descriptor so it doesn't get reused.
  fd_ = -1;
}

bool AwPrintManager::PrintNow() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* rfh = web_contents()->GetPrimaryMainFrame();
  if (!rfh->IsRenderFrameLive())
    return false;
  GetPrintRenderFrame(rfh)->PrintRequestedPages();
  return true;
}

void AwPrintManager::GetDefaultPrintSettings(
    GetDefaultPrintSettingsCallback callback) {
  // Unlike PrintViewManagerBase, we do process this in UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto params = printing::mojom::PrintParams::New();
  printing::RenderParamsFromPrintSettings(*settings_, params.get());
  params->document_cookie = cookie();
  if (!printing::PrintMsgPrintParamsIsValid(*params)) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(std::move(params));
}

void AwPrintManager::UpdateParam(
    std::unique_ptr<printing::PrintSettings> settings,
    int file_descriptor,
    PrintManager::PdfWritingDoneCallback callback) {
  DCHECK(settings);
  DCHECK(callback);
  settings_ = std::move(settings);
  fd_ = file_descriptor;
  set_pdf_writing_done_callback(std::move(callback));
  set_cookie(printing::PrintSettings::NewCookie());
}

void AwPrintManager::ScriptedPrint(
    printing::mojom::ScriptedPrintParamsPtr scripted_params,
    ScriptedPrintCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (scripted_params->is_scripted &&
      GetCurrentTargetFrame()->IsNestedWithinFencedFrame()) {
    DLOG(ERROR) << "Unexpected message received. Script Print is not allowed"
                   " in a fenced frame.";
    std::move(callback).Run(nullptr);
    return;
  }

  auto params = printing::mojom::PrintPagesParams::New();
  params->params = printing::mojom::PrintParams::New();
  printing::RenderParamsFromPrintSettings(*settings_, params->params.get());
  params->params->document_cookie = scripted_params->cookie;
  params->pages = settings_->ranges();

  if (!printing::PrintMsgPrintParamsIsValid(*params->params)) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(std::move(params));
}

void AwPrintManager::DidPrintDocument(
    printing::mojom::DidPrintDocumentParamsPtr params,
    DidPrintDocumentCallback callback) {
  if (params->document_cookie != cookie()) {
    std::move(callback).Run(false);
    return;
  }

  const printing::mojom::DidPrintContentParams& content = *params->content;
  if (!content.metafile_data_region.IsValid()) {
    NOTREACHED() << "invalid memory handle";
  }

  auto data = base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(
      content.metafile_data_region);
  if (!data) {
    NOTREACHED() << "couldn't map";
  }

  if (number_pages() > printing::kMaxPageCount) {
    web_contents()->Stop();
    PdfWritingDone(0);
    std::move(callback).Run(false);
    return;
  }

  DCHECK(pdf_writing_done_callback());
  base::ThreadPool::CreateTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})
      ->PostTaskAndReplyWithResult(
          FROM_HERE, base::BindOnce(&SaveDataToFd, fd_, number_pages(), data),
          base::BindOnce(&AwPrintManager::OnDidPrintDocumentWritingDone,
                         pdf_writing_done_callback(), std::move(callback)));
}

// static
void AwPrintManager::OnDidPrintDocumentWritingDone(
    const PdfWritingDoneCallback& callback,
    DidPrintDocumentCallback did_print_document_cb,
    uint32_t page_count) {
  DCHECK_LE(page_count, printing::kMaxPageCount);
  if (callback)
    callback.Run(base::checked_cast<int>(page_count));
  std::move(did_print_document_cb).Run(true);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AwPrintManager);

}  // namespace android_webview
