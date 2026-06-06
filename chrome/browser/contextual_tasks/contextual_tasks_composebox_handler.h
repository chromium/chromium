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
#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler_interface.h"
#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"
#include "components/contextual_tasks/public/query_contextualizer.h"
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

namespace contextual_tasks {
class ContextualTasksService;
class ContextualTasksUIInterface;
class DesktopQueryContextualizerDelegate;
}  // namespace contextual_tasks

// Struct to store file data and mime type.
struct FileData {
  std::string bytes;
  std::string mime_type;
  std::string name;
};

// ComposeboxHandler for the Contextual Tasks UI.
class ContextualTasksComposeboxHandler
    : public ComposeboxHandler,
      public ui::SelectFileDialog::Listener,
      public contextual_tasks::ContextualTasksComposeboxHandlerInterface {
 public:
  friend class ContextualTasksComposeboxHandlerTest;
  using TakeInputStateModelCallback = base::RepeatingCallback<
      std::unique_ptr<contextual_search::InputStateModel>()>;

  ContextualTasksComposeboxHandler(
      contextual_tasks::ContextualTasksUIInterface* web_ui_interface,
      Profile* profile,
      content::WebContents* web_contents,
      mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
      mojo::PendingRemote<composebox::mojom::Page> pending_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler,
      mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
      GetSessionHandleCallback get_session_callback,
      ClearSessionHandleCallback clear_session_callback,
      TakeInputStateModelCallback take_input_model_callback);
  ~ContextualTasksComposeboxHandler() override;

  // composebox::mojom::PageHandler:
  void SubmitQuery(const std::string& query_text,
                   uint8_t mouse_button,
                   bool alt_key,
                   bool ctrl_key,
                   bool meta_key,
                   bool shift_key,
                   bool is_voice_search) override;
  void DeleteContext(const base::UnguessableToken& file_token,
                     bool from_automatic_chip) override;
  void HandleFileUpload(bool is_image) override;
  void AddFileContext(searchbox::mojom::SelectedFileInfoPtr file_info,
                      mojo_base::BigBuffer file_bytes,
                      AddFileContextCallback callback) override;
  void AddTabContext(int32_t tab_id,
                     bool delay_upload,
                     AddTabContextCallback callback) override;
  void StartPlatformVoiceRecognition() override;

  // We override this method to inject an existing `InputStateModel` if one is
  // provided by the ContextualTasksUI via the `take_input_model_callback_`.
  void InitializeInputStateModel() override;

  void SetAimThreadRestoredTabs(
      std::vector<searchbox::mojom::TabInfoPtr> tabs) override;

  void AddFileContextFromBrowser(
      searchbox::mojom::SelectedFileInfoPtr file_info,
      AddFileContextCallback callback);

  // ContextualSearchboxHandler:

  void OnContextUploadStatusChanged(
      const base::UnguessableToken& context_token,
      lens::MimeType mime_type,
      contextual_search::ContextUploadStatus context_upload_status,
      const std::optional<contextual_search::ContextUploadErrorType>&
          error_type) override;

  void CreateAndSendQueryMessage(const std::string& query,
                                 bool is_voice_search);

  void ResetInputStateModel() override;
  void UpdateStateFromUrl(const GURL& url) override;
  void UpdateSuggestedTabContext(
      const contextual_tasks::SuggestedTabInfo* suggested_tab) override;
  void OnTaskChanged() override;

  std::vector<int32_t> GetSelectedTabIds() const override;

  void ClearFiles(bool should_block_auto_suggested_tabs) override;
#if !BUILDFLAG(IS_ANDROID)
  void HandleLensButtonClick() override;
  void OnLensThumbnailCreated(const std::string& thumbnail_data);
  virtual void CloseLensOverlay(
      lens::LensOverlayDismissalSource dismissal_source);
  void CloseLensOverlayFromWebUI(
      composebox::mojom::LensOverlayDismissalSource dismissal_source) override;
#endif

  // Callbacks for QueryContextualizer:

  // Called when the page context is determined to be ineligible for
  // contextualization (e.g., non-HTTP(S) URL).
  void OnPageContextIneligible();

  // Called when a tab has been processed for query contextualization.
  void OnTabProcessedForQueryContextualization(
      contextual_tasks::QueryContextualizer::TabId id);

  OmniboxController* GetOmniboxControllerForTesting() const {
    return omnibox_controller();
  }
  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void MultiFilesSelected(
      const std::vector<ui::SelectedFileInfo>& files) override;
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
#if !BUILDFLAG(IS_ANDROID)
  virtual std::optional<base::UnguessableToken> GetLensOverlayToken();
#endif

 private:
  // Returns the context ID for the active tab, if any.
  std::optional<int64_t> GetActiveTabContextId();

  // Whether to override the feature flag and force allow tab suggestions.
  // This is done when the initial active tab was already uploaded to initiate
  // the session as in Lens contextual queries.
  bool ShouldForceAllowTabSuggestion(int32_t tab_id);

  // Called when all tabs have been re-uploaded, to continue query
  // submission. `overlay_token` is the token of the initial objects request for
  // the Lens overlay / CSB, used in the ClientToAimRequest. It needs to be
  // passed at this point as by the time this function is called the Lens
  // overlay might have been closed.
  void ContinueCreateAndSendQueryMessage(
      std::string query,
      std::optional<base::Uuid> original_task_id,
      std::optional<base::UnguessableToken> overlay_token,
      bool is_voice_search);

#if !BUILDFLAG(IS_ANDROID)
  void OnVisualSelectionAdded(
      base::UnguessableToken overlay_token,
      base::expected<base::UnguessableToken,
                     contextual_search::ContextUploadErrorType> token);

  virtual LensSearchController* GetLensSearchController() const;
#endif  // !BUILDFLAG(IS_ANDROID)

  // Called when a non-delayed context upload (file or tab) has finished.
  // Potentially submits query if no other context is uploading.
  void MarkContextUploadFinished(const base::UnguessableToken& token);

  // Called when a delayed context upload (tab) has finished or was deleted.
  // Potentially submits query if no other context is uploading.
  void MarkDelayedTabUploadFinished(const int32_t tab_id);

  // Helper to send the pending query if all uploads are complete.
  void MaybeSendPendingQuery();

  TakeInputStateModelCallback take_input_model_callback_;
  raw_ptr<contextual_tasks::ContextualTasksUIInterface> web_ui_interface_;

  // The context controller for the current profile. The profile will outlive
  // this class.
  raw_ptr<contextual_tasks::ContextualTasksService> contextual_tasks_service_;

 protected:
  // Delegate handling desktop-specific operations for QueryContextualizer,
  // such as tab validation and retrieving viewport encoding options.
  std::unique_ptr<contextual_tasks::DesktopQueryContextualizerDelegate>
      desktop_delegate_;

 private:
  std::unique_ptr<contextual_tasks::QueryContextualizer> recontextualizer_;
  scoped_refptr<ui::SelectFileDialog> file_dialog_;
  // Map of context tokens to tab IDs for tabs that are delayed for upload.
  // These tabs will be contextualized and added to the context after user
  // submits the query in the composebox.
  std::map<base::UnguessableToken, int32_t> delayed_tabs_;

  // The pending query request info to be sent once uploads are complete.
  std::unique_ptr<contextual_search::ContextualSearchContextController::
                      CreateClientToAimRequestInfo>
      pending_query_request_info_;

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

  // Number of recontextualization flows currently in progress.
  int recontextualization_pending_count_ = 0;

#if !BUILDFLAG(IS_ANDROID)
  // The token associated with the visual selection. This does not actually
  // correspond to a real file upload, but is used to represent the visual
  // selection in the UI and in the event that the user submits a query with
  // the visual selection. The visual selection request flow is handled by
  // the Lens.
  std::optional<base::UnguessableToken> visual_selection_token_;

  // The overlay token associated with the visual selection. This is stored
  // alongside the visual selection token because the overlay controller may be
  // reset or closed, but the visual selection should still be associated with
  // the overlay token that created it.
  std::optional<base::UnguessableToken> visual_selection_overlay_token_;
#endif
  base::WeakPtrFactory<ContextualTasksComposeboxHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COMPOSEBOX_HANDLER_H_
