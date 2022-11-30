// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/file_manager/file_manager_page_handler.h"

#include "ash/webui/file_manager/file_manager_ui.h"

namespace ash {
namespace file_manager {

FileManagerPageHandler::FileManagerPageHandler(
    FileManagerUI* file_manager_ui,
    mojo::PendingReceiver<mojom::PageHandler> pending_receiver,
    mojo::PendingRemote<mojom::Page> pending_page)
    : file_manager_ui_(file_manager_ui),
      receiver_(this, std::move(pending_receiver)),
      page_(std::move(pending_page)) {
  DCHECK(file_manager_ui_);
}

FileManagerPageHandler::~FileManagerPageHandler() = default;

}  // namespace file_manager
}  // namespace ash
