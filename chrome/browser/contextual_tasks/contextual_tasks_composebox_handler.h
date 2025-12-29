// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COMPOSEBOX_HANDLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COMPOSEBOX_HANDLER_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

class Profile;
class ContextualTasksUI;
class LensSearchController;

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace contextual_tasks {
struct ContextualTaskContext;
class ContextualTasksService;
struct UrlAttachment;
}  // namespace contextual_tasks

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
  friend class ContextualTasksComposeboxHandlerTest;
  ContextualTasksComposeboxHandler(
      ContextualTasksUI* ui_controller,
      Profile* profile,
      content::WebContents* web_contents,
      mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
      mojo::PendingRemote<composebox::mojom::Page> pending_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler,
      GetSessionHandleCallback get_session_callback);
  ~ContextualTasksComposeboxHandler() override;

  // composebox::mojom::PageHandler:
  void SubmitQuery(const std::string& query_text,
                   uint8_t mouse_button,
                   bool alt_key,
                   bool ctrl_key,
                   bool meta_key,
                   bool shift_key) override;
  void ClearFiles() override;
  void DeleteContext(const base::UnguessableToken& file_token,
                     bool from_automatic_chip) override;
  void HandleFileUpload(bool is_image) override;
  void AddFileContext(searchbox::mojom::SelectedFileInfoPtr file_info,
                      mojo_base::BigBuffer file_bytes,
                      AddFileContextCallback callback) override;
  void AddTabContext(int32_t tab_id,
                     bool delay_upload,
                     AddTabContextCallback callback) override;

  // ContextualSearchboxHandler:
  void OnFileUploadStatusChanged(
      const base::UnguessableToken& file_token,
      lens::MimeType mime_type,
      contextual_search::FileUploadStatus file_upload_status,
      const std::optional<contextual_search::FileUploadErrorType>& error_type)
      override;

  void CreateAndSendQueryMessage(const std::string& query);

  void HandleLensButtonClick() override;
  void OnLensThumbnailCreated(const std::string& thumbnail_data);
  virtual void CloseLensOverlay(
      lens::LensOverlayDismissalSource dismissal_source);

  OmniboxController* GetOmniboxControllerForTesting() const {
    return omnibox_controller();
  }
  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;
  void OnFileRead(std::unique_ptr<FileData> file_data);

 protected:
  virtual contextual_tasks::ContextualTasksService* GetContextualTasksService();

 private:
  void OnFileAddedToSession(searchbox::mojom::SelectedFileInfoPtr file_info,
                            AddFileContextCallback callback,
                            const base::UnguessableToken& token);

  // Called when the context is retrieved from the context service, for
  // determining which tabs need to be re-uploaded before query submission via
  // CreateAndSendQueryMessage.
  void OnContextRetrieved(
      std::string query,
      tabs::TabHandle active_tab_handle,
      std::unique_ptr<contextual_tasks::ContextualTaskContext> context);

  // Called when a tab context has been re-uploaded, to continue query
  // submission.
  void OnTabContextReuploaded(std::string query,
                              base::RepeatingClosure barrier_closure,
                              bool success);

  // Called when all tabs have been re-uploaded, to continue query
  // submission.
  void ContinueCreateAndSendQueryMessage(std::string query);

  // Returns the tabs that need to be re-uploaded before query submission based
  // on the tabs present in the context.
  std::vector<tabs::TabInterface*> GetTabsToUpdate(
      const contextual_tasks::ContextualTaskContext& context,
      tabs::TabInterface* active_tab);

  // Returns a context id for the given tab from the query controller, or
  // std::nullopt if not found.
  std::optional<int64_t> GetContextIdForTab(
      const contextual_tasks::ContextualTaskContext& context,
      const lens::ContextualInputData& page_content_data);

  // Called when a tab contextualization has been fetched, to re-upload the
  // tab context.
  void OnTabContextualizationFetched(
      std::string query,
      std::unique_ptr<contextual_tasks::ContextualTaskContext> context,
      base::RepeatingClosure barrier_closure,
      int32_t tab_id,
      std::unique_ptr<lens::ContextualInputData> page_content_data);

  void OnVisualSelectionAdded(const base::UnguessableToken& token);

  LensSearchController* GetLensSearchController() const;

  // Returns the matching attachment for the given URL and session ID.
  const contextual_tasks::UrlAttachment* GetMatchingAttachment(
      const contextual_tasks::ContextualTaskContext& context,
      const GURL& url,
      SessionID session_id);

  // Returns true if the tab context should be uploaded based on the context ID
  // and page content data.
  bool ShouldUploadTabContext(
      std::optional<int64_t> context_id,
      const lens::ContextualInputData& page_content_data);

  raw_ptr<ContextualTasksUI> web_ui_controller_;
  // The context controller for the current profile. The profile will outlive
  // this class.
  raw_ptr<contextual_tasks::ContextualTasksService> contextual_tasks_service_;
  scoped_refptr<ui::SelectFileDialog> file_dialog_;
  // Map of context tokens to tab IDs for tabs that are delayed for upload.
  // These tabs will be contextualized and added to the context after user
  // submits the query in the composebox.
  std::map<base::UnguessableToken, int32_t> delayed_tabs_;

  std::optional<base::UnguessableToken> visual_selection_token_;
  base::WeakPtrFactory<ContextualTasksComposeboxHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COMPOSEBOX_HANDLER_H_
