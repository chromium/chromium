// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_FILE_MANAGER_FILE_MANAGER_PAGE_HANDLER_H_
#define ASH_WEBUI_FILE_MANAGER_FILE_MANAGER_PAGE_HANDLER_H_

#include "ash/webui/file_manager/mojom/file_manager.mojom.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
namespace file_manager {

class FileManagerUI;

// Class backing the page's functionality.
class FileManagerPageHandler : public mojom::PageHandler {
 public:
  FileManagerPageHandler(
      FileManagerUI* file_manager_ui,
      mojo::PendingReceiver<mojom::PageHandler> pending_receiver,
      mojo::PendingRemote<mojom::Page> pending_page);
  ~FileManagerPageHandler() override;

  FileManagerPageHandler(const FileManagerPageHandler&) = delete;
  FileManagerPageHandler& operator=(const FileManagerPageHandler&) = delete;

 private:
  raw_ptr<FileManagerUI> file_manager_ui_;  // Owns |this|.
  mojo::Receiver<mojom::PageHandler> receiver_;
  mojo::Remote<mojom::Page> page_;
};

}  // namespace file_manager
}  // namespace ash

#endif  // ASH_WEBUI_FILE_MANAGER_FILE_MANAGER_PAGE_HANDLER_H_
