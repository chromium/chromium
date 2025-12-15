// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COMPOSEBOX_HANDLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COMPOSEBOX_HANDLER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

class Profile;
class ContextualTasksUI;

// Struct to store file data and mime type.
struct FileData {
  std::string bytes;
  std::string mime_type;
  std::string name;
};

// ComposeboxHandler for the Contextual Tasks UI.
class ContextualTasksComposeboxHandler : public ComposeboxHandler,
                                         public ui::SelectFileDialog::Listener {
 public:
  ContextualTasksComposeboxHandler(
      ContextualTasksUI* ui_controller,
      Profile* profile,
      content::WebContents* web_contents,
      mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
      mojo::PendingRemote<composebox::mojom::Page> pending_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler);
  ~ContextualTasksComposeboxHandler() override;

  // composebox::mojom::PageHandler:
  void SubmitQuery(const std::string& query_text,
                   uint8_t mouse_button,
                   bool alt_key,
                   bool ctrl_key,
                   bool meta_key,
                   bool shift_key) override;
  void DeleteContext(const base::UnguessableToken& file_token,
                     bool from_automatic_chip) override;
  void HandleFileUpload(bool is_image) override;
  void AddFileContext(searchbox::mojom::SelectedFileInfoPtr file_info,
                      mojo_base::BigBuffer file_bytes,
                      AddFileContextCallback callback) override;

  void CreateAndSendQueryMessage(const std::string& query);

  OmniboxController* GetOmniboxControllerForTesting() {
    return omnibox_controller();
  }
  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;
  void OnFileRead(std::unique_ptr<FileData> file_data);

 private:
  void OnFileAddedToSession(searchbox::mojom::SelectedFileInfoPtr file_info,
                            AddFileContextCallback callback,
                            const base::UnguessableToken& token);

  raw_ptr<ContextualTasksUI> web_ui_controller_;
  scoped_refptr<ui::SelectFileDialog> file_dialog_;

  base::WeakPtrFactory<ContextualTasksComposeboxHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COMPOSEBOX_HANDLER_H_
