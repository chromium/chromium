// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/media_app_ui/media_app_page_handler.h"

#include <utility>

#include "ash/webui/media_app_ui/file_system_access_helpers.h"
#include "ash/webui/media_app_ui/media_app_ui.h"
#include "ash/webui/media_app_ui/media_app_ui_delegate.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_util.h"
#include "url/origin.h"

namespace ash {

namespace {

void IsFileURLBrowserWritable(
    MediaAppPageHandler::IsFileBrowserWritableCallback callback,
    std::optional<storage::FileSystemURL> url) {
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

void MediaAppPageHandler::IsFileArcWritable(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
    IsFileArcWritableCallback callback) {
  media_app_ui_->delegate()->IsFileArcWritable(std::move(token),
                                               std::move(callback));
}

void MediaAppPageHandler::IsFileBrowserWritable(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
    IsFileBrowserWritableCallback callback) {
  ash::ResolveTransferToken(
      std::move(token), media_app_ui_->web_ui()->GetWebContents(),
      base::BindOnce(&IsFileURLBrowserWritable, std::move(callback)));
}

void MediaAppPageHandler::EditInPhotos(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
    const std::string& mime_type,
    EditInPhotosCallback callback) {
  media_app_ui_->delegate()->EditInPhotos(std::move(token), mime_type,
                                          std::move(callback));
}

void MediaAppPageHandler::SubmitForm(const GURL& url,
                                     const std::vector<int8_t>& payload,
                                     const std::string& header,
                                     SubmitFormCallback callback) {
  // We only intend for this API to be used with lens, so crash if used for
  // something else.
  const url::Origin kLensOrigin =
      url::Origin::Create(GURL("https://lens.google.com"));
  if (!kLensOrigin.IsSameOriginWith(url) ||
      !base::StartsWith(
          header, "Content-Type:", base::CompareCase::INSENSITIVE_ASCII) ||
      !net::HttpUtil::IsValidHeaderValue(header)) {
    receiver_.ReportBadMessage(
        "SubmitForm API only works with https://lens.google.com and a single "
        "Content-Type header");
    return;
  }
  media_app_ui_->delegate()->SubmitForm(url, payload, header);
  std::move(callback).Run();
}

}  // namespace ash
