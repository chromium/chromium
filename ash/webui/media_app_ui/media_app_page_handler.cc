// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/media_app_ui/media_app_page_handler.h"

#include <utility>

#include "ash/webui/media_app_ui/media_app_ui.h"
#include "ash/webui/media_app_ui/media_app_ui_delegate.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/file_system_access_entry_factory.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"

namespace ash {

namespace {

void IsFileURLBrowserWritable(
    MediaAppPageHandler::IsFileBrowserWritableCallback callback,
    absl::optional<storage::FileSystemURL> url) {
  if (!url.has_value()) {
    std::move(callback).Run(false);
    return;
  };

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::PathIsWritable, url->path()), std::move(callback));
}

}  // namespace

MediaAppPageHandler::MediaAppPageHandler(
    MediaAppUI* media_app_ui,
    mojo::PendingReceiver<media_app_ui::mojom::PageHandler> receiver)
    : receiver_(this, std::move(receiver)), media_app_ui_(media_app_ui) {}

MediaAppPageHandler::~MediaAppPageHandler() = default;

void MediaAppPageHandler::OpenFeedbackDialog(
    OpenFeedbackDialogCallback callback) {
  auto error_message = media_app_ui_->delegate()->OpenFeedbackDialog();
  std::move(callback).Run(std::move(error_message));
}

void MediaAppPageHandler::ToggleBrowserFullscreenMode(
    ToggleBrowserFullscreenModeCallback callback) {
  media_app_ui_->delegate()->ToggleBrowserFullscreenMode();
  std::move(callback).Run();
}

void MediaAppPageHandler::IsFileBrowserWritable(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
    IsFileBrowserWritableCallback callback) {
  auto* web_contents = media_app_ui_->web_ui()->GetWebContents();
  web_contents->GetBrowserContext()
      ->GetStoragePartition(web_contents->GetSiteInstance())
      ->GetFileSystemAccessEntryFactory()
      ->ResolveTransferToken(
          std::move(token),
          base::BindOnce(&IsFileURLBrowserWritable, std::move(callback)));
}

void MediaAppPageHandler::EditInPhotos(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
    const std::string& mime_type,
    EditInPhotosCallback callback) {
  auto* web_contents = media_app_ui_->web_ui()->GetWebContents();
  web_contents->GetBrowserContext()
      ->GetStoragePartition(web_contents->GetSiteInstance())
      ->GetFileSystemAccessEntryFactory()
      ->ResolveTransferToken(
          std::move(token),
          base::BindOnce(
              [](base::WeakPtr<MediaAppUIDelegate> delegate,
                 EditInPhotosCallback inner_callback,
                 const std::string& mime_type,
                 absl::optional<storage::FileSystemURL> url) {
                if (delegate) {
                  delegate->EditFileInPhotos(url, mime_type,
                                             std::move(inner_callback));
                }
              },
              media_app_ui_->delegate()->GetWeakPtr(), std::move(callback),
              mime_type));
}

}  // namespace ash
