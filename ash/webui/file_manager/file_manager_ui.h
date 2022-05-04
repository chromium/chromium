// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_FILE_MANAGER_FILE_MANAGER_UI_H_
#define ASH_WEBUI_FILE_MANAGER_FILE_MANAGER_UI_H_

#include <memory>

#include "ash/webui/file_manager/file_manager_ui_delegate.h"
#include "ash/webui/file_manager/mojom/file_manager.mojom.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace ash {
namespace file_manager {

class FileManagerPageHandler;

// WebUI controller for chrome://file-manager.
class FileManagerUI : public ui::MojoWebDialogUI,
                      public mojom::PageHandlerFactory {
 public:
  FileManagerUI(content::WebUI* web_ui,
                std::unique_ptr<FileManagerUIDelegate> delegate);
  ~FileManagerUI() override;

  FileManagerUI(const FileManagerUI&) = delete;
  FileManagerUI& operator=(const FileManagerUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<mojom::PageHandlerFactory> pending_receiver);

  const FileManagerUIDelegate* delegate() { return delegate_.get(); }

  // Get the number of open File Manager windows.
  // Should be called on UI thread.
  static int GetNumInstances();

 private:
  content::WebUIDataSource* CreateTrustedAppDataSource();

  // mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<mojom::Page> pending_page,
      mojo::PendingReceiver<mojom::PageHandler> pending_page_handler) override;

  std::unique_ptr<FileManagerUIDelegate> delegate_;

  mojo::Receiver<mojom::PageHandlerFactory> page_factory_receiver_{this};
  std::unique_ptr<FileManagerPageHandler> page_handler_;

  static inline int num_instances_ = 0;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace file_manager
}  // namespace ash

#endif  // ASH_WEBUI_FILE_MANAGER_FILE_MANAGER_UI_H_
