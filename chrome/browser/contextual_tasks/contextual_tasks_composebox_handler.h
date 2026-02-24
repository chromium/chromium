// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COMPOSEBOX_HANDLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COMPOSEBOX_HANDLER_H_

#include <map>
#include <string>
#include <vector>

#include "base/functional/callback.h"
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
class LensSearchController;

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace contextual_tasks {
struct ContextualTaskContext;
class ContextualTasksService;
class ContextualTasksUIInterface;
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
  using GetInputStateModelCallback =
      base::OnceCallback<std::unique_ptr<contextual_search::InputStateModel>()>;

  ContextualTasksComposeboxHandler(
      contextual_tasks::ContextualTasksUIInterface* web_ui_interface,
      Profile* profile,
      content::WebContents* web_contents,
      mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
      mojo::PendingRemote<composebox::mojom::Page> pending_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler,
      GetSessionHandleCallback get_session_callback,
      GetInputStateModelCallback get_input_model_callback);
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
  void AddTabContext(int32_t tab_id,
                     bool delay_upload,
                     AddTabContextCallback callback) override;

  void OnTaskChanged();

  // We override this method to inject an existing `InputStateModel` if one is
  // provided by the ContextualTasksUI via the `get_input_model_callback_`.
  void InitializeInputStateModel() override;

  void AddFileContextFromBrowser(
      searchbox::mojom::SelectedFileInfoPtr file_info,
      AddFileContextCallback callback);

  // ContextualSearchboxHandler:

  void OnFileUploadStatusChanged(
      const base::UnguessableToken& file_token,
      lens::MimeType mime_type,
      contextual_search::FileUploadStatus file_upload_status,
      const std::optional<contextual_search::FileUploadErrorType>& error_type)
      override;

  void CreateAndSendQueryMessage(const std::string& query);

  // Called to update the suggested tab context chip in the compose box based on
  // the given candidate tab. The chip will only be shown if the candidate tab
  // is eligible for suggestion and is not blocklisted by the user.
  virtual void UpdateSuggestedTabContext(
      searchbox::mojom::TabInfoPtr candidate_tab_info);

  // Returns true if there is a suggested tab context chip in the compose box.
  bool has_suggested_tab_context() const {
    return current_suggestion_.has_value();
  }

  // Called to clear the blocklist of auto-suggested tabs. This is used when
  // switching to a new thread.
  void ResetBlocklistedSuggestions() { blocklisted_suggestions_.clear(); }

  void ClearFiles(bool should_block_auto_suggested_tabs) override;
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

  // Helper to check if any context tokens are currently uploading.
  bool IsAnyContextUploading();

  // Helper to check if there is a stashed query not submitted to AIM yet.
  bool HasPendingQueryForTesting() const;

  uint16_t GetNumTabsDelayed() const;
  uint16_t GetNumContextUploading() const;

 protected:
  virtual contextual_tasks::ContextualTasksService* GetContextualTasksService();
  virtual std::optional<base::UnguessableToken> GetLensOverlayToken();

 private:
  // Called when a non-delayed context upload (file or tab) has finished.
  // Potentially submits query if no other context is uploading.
  void MarkContextUploadFinished(const base::UnguessableToken& token);

  GetInputStateModelCallback get_input_model_callback_;

  // Called when a delayed context upload (tab) has finished.
  // Potentially submits query if no other context is uploading.
  void MarkDelayedTabUploadFinished(const int32_t tab_id);

  // Called when the context is retrieved from the context service, for
  // determining which tabs need to be re-uploaded before query submission via
  // CreateAndSendQueryMessage.
  void OnContextRetrieved(
      std::string query,
      tabs::TabHandle active_tab_handle,
      std::optional<base::Uuid> original_task_id,
      std::optional<base::UnguessableToken> overlay_token,
      std::unique_ptr<contextual_tasks::ContextualTaskContext> context);

  // Called when a tab context reupload has started or canceled, to continue
  // query submission.
  void OnTabContextReuploadStarted(base::RepeatingClosure barrier_closure,
                                   std::optional<base::Uuid> original_task_id,
                                   bool upload_started);

  // Called when all tabs have been re-uploaded, to continue query
  // submission. `overlay_token` is the token of the initial objects request for
  // the Lens overlay / CSB, used in the ClientToAimRequest. It needs to be
  // passed at this point as by the time this function is called the Lens
  // overlay might have been closed.
  void ContinueCreateAndSendQueryMessage(
      std::string query,
      std::optional<base::Uuid> original_task_id,
      std::optional<base::UnguessableToken> overlay_token);

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
      std::unique_ptr<contextual_tasks::ContextualTaskContext> context,
      base::RepeatingClosure barrier_closure,
      std::optional<base::Uuid> original_task_id,
      int32_t tab_id,
      std::unique_ptr<lens::ContextualInputData> page_content_data);

  void OnVisualSelectionAdded(
      base::UnguessableToken overlay_token,
      base::expected<base::UnguessableToken,
                     contextual_search::FileUploadErrorType> token);

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

  // Returns the context ID for the active tab, if any.
  std::optional<int64_t> GetActiveTabContextId();

  raw_ptr<contextual_tasks::ContextualTasksUIInterface> web_ui_interface_;
  // Cleanup once a single tab finishes uploading.
  void OnSingleTabProcessed(base::RepeatingClosure barrier_closure,
                            int32_t tab_id);

  // Helper to send the pending query if all uploads are complete.
  void MaybeSendPendingQuery();

  // Sends an update to AIM that an injected input has been deleted.
  void SendDeleteInjectedInputUpdate(const std::string& id);

  // The context controller for the current profile. The profile will outlive
  // this class.
  raw_ptr<contextual_tasks::ContextualTasksService> contextual_tasks_service_;
  scoped_refptr<ui::SelectFileDialog> file_dialog_;
  // Map of context tokens to tab IDs for tabs that are delayed for upload.
  // These tabs will be contextualized and added to the context after user
  // submits the query in the composebox.
  std::map<base::UnguessableToken, int32_t> delayed_tabs_;

  // List of auto-suggested tab URLs that have been explicitly dismissed by the
  // user. Those URLs will not be auto-suggested again for the same task in the
  // same session, unless the user explicitly adds the tab via "+" button or
  // switches to a new thread in which case the whole list will be cleared.
  std::set<GURL> blocklisted_suggestions_;

  // The URL of the current suggested tab context.
  std::optional<GURL> current_suggestion_;

  // The message to be sent to the webview once uploads are complete.
  std::optional<lens::ClientToAimMessage> pending_message_;

  // Set of tabs still delayed. Is set of tab id's, while `delayed_tabs_`
  // is map of token to tab id. We do not always have access to file token
  // (for example, active tab), so we need this separate set to track
  // which tabs are still delayed based on tab ids. `delayed_tabs_`
  // is also cleared when tabs are moved into `tabs_to_update`
  // (queue to be uploaded), but the tabs in this set remain for longer,
  // until the callback after uploading is called.
  std::set<int32_t> pending_delayed_tab_ids_;

  // Includes normal tabs and files still uploading, but not delayed tabs.
  std::set<base::UnguessableToken> pending_context_uploads_;

  std::optional<base::UnguessableToken> visual_selection_token_;
  // The overlay token associated with the visual selection. This is stored
  // alongside the visual selection token because the overlay controller may be
  // reset or closed, but the visual selection should still be associated with
  // the overlay token that created it.
  std::optional<base::UnguessableToken> visual_selection_overlay_token_;
  base::WeakPtrFactory<ContextualTasksComposeboxHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COMPOSEBOX_HANDLER_H_
