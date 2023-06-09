// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_FILE_MANAGER_FILE_MANAGER_UI_H_
#define ASH_WEBUI_FILE_MANAGER_FILE_MANAGER_UI_H_

#include <memory>

#include "ash/webui/file_manager/file_manager_ui_delegate.h"
#include "ash/webui/file_manager/mojom/file_manager.mojom.h"
#include "ash/webui/system_apps/public/system_web_app_ui_config.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ui {
class ColorChangeHandler;
}

namespace ash {
namespace file_manager {

class FileManagerPageHandler;
class FileManagerUI;

// The WebUIConfig for chrome://file-manager.
class FileManagerUIConfig : public SystemWebAppUIConfig<FileManagerUI> {
 public:
  explicit FileManagerUIConfig(
      SystemWebAppUIConfig::CreateWebUIControllerFunc create_controller_func);

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI for chrome://file-manager.
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

  // Instantiates the implementor of the mojom::PageHandler mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

  // Get the number of open File Manager windows.
  // Should be called on UI thread.
  static int GetNumInstances();

 private:
  void CreateAndAddTrustedAppDataSource(content::WebUI* web_ui,
                                        int window_number);

  // mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<mojom::Page> pending_page,
      mojo::PendingReceiver<mojom::PageHandler> pending_page_handler) override;

  std::unique_ptr<FileManagerUIDelegate> delegate_;

  mojo::Receiver<mojom::PageHandlerFactory> page_factory_receiver_{this};
  std::unique_ptr<FileManagerPageHandler> page_handler_;

  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  // Counts the number of active Files SWA instances. This counter goes up every
  // time a new window is opened and down every time a window is closed.
  static inline int instance_count_ = 0;

  // Counts the total number of windows opened. Unlike the instance_count_ this
  // counter never is decremented.
  static inline int window_counter_ = 0;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace file_manager
}  // namespace ash

#endif  // ASH_WEBUI_FILE_MANAGER_FILE_MANAGER_UI_H_
