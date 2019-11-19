// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_print_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "components/printing/browser/print_manager_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"

namespace android_webview {

namespace {

int SaveDataToFd(int fd,
                 int page_count,
                 scoped_refptr<base::RefCountedSharedMemoryMapping> data) {
  bool result = fd > base::kInvalidFd &&
                base::IsValueInRangeForNumericType<int>(data->size());
  if (result) {
    int size = data->size();
    result = base::WriteFileDescriptor(fd, data->front_as<char>(), size);
  }
  return result ? page_count : 0;
}

}  // namespace

// static
AwPrintManager* AwPrintManager::CreateForWebContents(
    content::WebContents* contents,
    std::unique_ptr<printing::PrintSettings> settings,
    int file_descriptor,
    PrintManager::PdfWritingDoneCallback callback) {
  AwPrintManager* print_manager = new AwPrintManager(
      contents, std::move(settings), file_descriptor, std::move(callback));
  contents->SetUserData(UserDataKey(), base::WrapUnique(print_manager));
  return print_manager;
}

AwPrintManager::AwPrintManager(
    content::WebContents* contents,
    std::unique_ptr<printing::PrintSettings> settings,
    int file_descriptor,
    PdfWritingDoneCallback callback)
    : PrintManager(contents),
      settings_(std::move(settings)),
      fd_(file_descriptor) {
  DCHECK(settings_);
  pdf_writing_done_callback_ = std::move(callback);
  DCHECK(pdf_writing_done_callback_);
  cookie_ = 1;  // Set a valid dummy cookie value.
}

AwPrintManager::~AwPrintManager() = default;

void AwPrintManager::PdfWritingDone(int page_count) {
  pdf_writing_done_callback_.Run(page_count);
  // Invalidate the file descriptor so it doesn't get reused.
  fd_ = -1;
}

bool AwPrintManager::PrintNow() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* rfh = web_contents()->GetMainFrame();
  GetPrintRenderFrame(rfh)->PrintRequestedPages();
  return true;
}

void AwPrintManager::OnGetDefaultPrintSettings(
    content::RenderFrameHost* render_frame_host,
    IPC::Message* reply_msg) {
  // Unlike the printing_message_filter, we do process this in UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PrintMsg_Print_Params params;
  printing::RenderParamsFromPrintSettings(*settings_, &params);
  params.document_cookie = cookie_;
  PrintHostMsg_GetDefaultPrintSettings::WriteReplyParams(reply_msg, params);
  render_frame_host->Send(reply_msg);
}

void AwPrintManager::OnScriptedPrint(
    content::RenderFrameHost* render_frame_host,
    const PrintHostMsg_ScriptedPrint_Params& scripted_params,
    IPC::Message* reply_msg) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PrintMsg_PrintPages_Params params;
  printing::RenderParamsFromPrintSettings(*settings_, &params.params);
  params.params.document_cookie = scripted_params.cookie;
  params.pages = printing::PageRange::GetPages(settings_->ranges());
  PrintHostMsg_ScriptedPrint::WriteReplyParams(reply_msg, params);
  render_frame_host->Send(reply_msg);
}

void AwPrintManager::OnDidPrintDocument(
    content::RenderFrameHost* render_frame_host,
    const PrintHostMsg_DidPrintDocument_Params& params,
    std::unique_ptr<DelayedFrameDispatchHelper> helper) {
  if (params.document_cookie != cookie_)
    return;

  const PrintHostMsg_DidPrintContent_Params& content = params.content;
  if (!content.metafile_data_region.IsValid()) {
    NOTREACHED() << "invalid memory handle";
    web_contents()->Stop();
    PdfWritingDone(0);
    return;
  }

  auto data = base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(
      content.metafile_data_region);
  if (!data) {
    NOTREACHED() << "couldn't map";
    web_contents()->Stop();
    PdfWritingDone(0);
    return;
  }

  DCHECK(pdf_writing_done_callback_);
  base::PostTaskAndReplyWithResult(
      base::CreateTaskRunner({base::ThreadPool(), base::MayBlock(),
                              base::TaskPriority::BEST_EFFORT,
                              base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})
          .get(),
      FROM_HERE, base::BindOnce(&SaveDataToFd, fd_, number_pages_, data),
      base::BindOnce(&AwPrintManager::OnDidPrintDocumentWritingDone,
                     pdf_writing_done_callback_, std::move(helper)));
}

// static
void AwPrintManager::OnDidPrintDocumentWritingDone(
    const PdfWritingDoneCallback& callback,
    std::unique_ptr<DelayedFrameDispatchHelper> helper,
    int page_count) {
  if (callback)
    callback.Run(page_count);
  helper->SendCompleted();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AwPrintManager)

}  // namespace android_webview
