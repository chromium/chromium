// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_commands.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/common/google_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/url_util.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/download/download_target_determiner.h"
#endif

namespace {

// Maximum size (compressed) of image to be copied to the clipboard. If the
// image exceeds this size, the image is not copied.
const int64_t kMaxImageClipboardSize = 20 * 1024 * 1024;  // 20 MB

class ImageClipboardCopyManager : public ImageDecoder::ImageRequest {
 public:
  ImageClipboardCopyManager() = delete;
  ImageClipboardCopyManager(const ImageClipboardCopyManager&) = delete;
  ImageClipboardCopyManager& operator=(const ImageClipboardCopyManager&) =
      delete;

  static void Start(const base::FilePath& file_path,
                    base::SequencedTaskRunner* task_runner) {
    new ImageClipboardCopyManager(file_path, task_runner);
  }

 private:
  ImageClipboardCopyManager(const base::FilePath& file_path,
                            base::SequencedTaskRunner* task_runner)
      : file_path_(file_path) {
    // Constructor must be called in the UI thread.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&ImageClipboardCopyManager::StartDecoding,
                                  base::Unretained(this)));
  }

  void StartDecoding() {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);

    // Re-check the filesize since the file may be modified after downloaded.
    int64_t filesize;
    if (!GetFileSize(file_path_, &filesize) ||
        filesize > kMaxImageClipboardSize) {
      OnFailedBeforeDecoding();
      return;
    }

    std::string data;
    bool ret = base::ReadFileToString(file_path_, &data);
    if (!ret || data.empty()) {
      OnFailedBeforeDecoding();
      return;
    }

    // Note: An image over 128MB (uncompressed) may fail, due to the limitation
    // of IPC message size.
    ImageDecoder::Start(this, std::move(data));
  }

  void OnImageDecoded(const SkBitmap& decoded_image) override {
    // This method is called on the same thread as constructor (the UI thread).
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.Reset();

    if (!decoded_image.empty() && !decoded_image.isNull())
      scw.WriteImage(decoded_image);

    delete this;
  }

  void OnDecodeImageFailed() override {
    // This method is called on the same thread as constructor (the UI thread).
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    delete this;
  }

  void OnFailedBeforeDecoding() {
    // We don't need to cancel the job, since it shouldn't be started here.

    task_runner()->DeleteSoon(FROM_HERE, this);
  }

  const base::FilePath file_path_;
};

}  // namespace

DownloadCommands::DownloadCommands(base::WeakPtr<DownloadUIModel> model)
    : model_(model) {
  DCHECK(model_);
}

DownloadCommands::~DownloadCommands() = default;

GURL DownloadCommands::GetLearnMoreURLForInterruptedDownload() const {
  if (!model_)
    return GURL();

  GURL learn_more_url(chrome::kDownloadInterruptedLearnMoreURL);
  learn_more_url = google_util::AppendGoogleLocaleParam(
      learn_more_url, g_browser_process->GetApplicationLocale());
  return net::AppendQueryParameter(
      learn_more_url, "ctx",
      base::NumberToString(model_->GetDownloadItem()->GetLastReason()));
}

bool DownloadCommands::IsCommandEnabled(Command command) const {
  return model_ ? model_->IsCommandEnabled(this, command) : false;
}

bool DownloadCommands::IsCommandChecked(Command command) const {
  return model_ ? model_->IsCommandChecked(this, command) : false;
}

bool DownloadCommands::IsCommandVisible(Command command) const {
  if (!model_)
    return false;

  if (command == PLATFORM_OPEN)
    return model_->ShouldPreferOpeningInBrowser();

  return true;
}

void DownloadCommands::ExecuteCommand(Command command) {
  if (!model_)
    return;

  model_->ExecuteCommand(this, command);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)

Browser* DownloadCommands::GetBrowser() const {
  if (!model_)
    return nullptr;

  chrome::ScopedTabbedBrowserDisplayer browser_displayer(model_->profile());
  DCHECK(browser_displayer.browser());
  return browser_displayer.browser();
}

bool DownloadCommands::IsDownloadPdf() const {
  if (!model_)
    return false;

  base::FilePath path = model_->GetTargetFilePath();
  return path.MatchesExtension(FILE_PATH_LITERAL(".pdf"));
}

bool DownloadCommands::CanOpenPdfInSystemViewer() const {
  return IsDownloadPdf();
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

void DownloadCommands::CopyFileAsImageToClipboard() {
  if (!model_)
    return;

  if (model_->GetState() != download::DownloadItem::COMPLETE ||
      model_->GetCompletedBytes() > kMaxImageClipboardSize) {
    return;
  }

  if (!model_->HasSupportedImageMimeType())
    return;

  base::FilePath file_path = model_->GetFullPath();

  if (!task_runner_) {
    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  }
  ImageClipboardCopyManager::Start(file_path, task_runner_.get());
}

bool DownloadCommands::CanBeCopiedToClipboard() const {
  if (!model_)
    return false;

  return model_->GetState() == download::DownloadItem::COMPLETE &&
         model_->GetCompletedBytes() <= kMaxImageClipboardSize;
}
