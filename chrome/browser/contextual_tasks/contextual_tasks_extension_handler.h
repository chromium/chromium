// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EXTENSION_HANDLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EXTENSION_HANDLER_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_types.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/window_open_disposition.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"
#include "url/gurl.h"

// Handles Mojo connection requests from the Contextual Tasks extension
// document, implementing composebox and searchbox page handler interfaces.
class ContextualTasksExtensionHandler
    : public content::DocumentUserData<ContextualTasksExtensionHandler>,
      public composebox::mojom::PageHandlerFactory,
      public composebox::mojom::PageHandler,
      public searchbox::mojom::PageHandler {
 public:
  ~ContextualTasksExtensionHandler() override;

  void BindComposeboxFactory(
      mojo::PendingReceiver<composebox::mojom::PageHandlerFactory> receiver);

  // composebox::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<composebox::mojom::PageHandler> receiver,
      mojo::PendingRemote<searchbox::mojom::Page> searchbox_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler> searchbox_handler)
      override;

  // composebox::mojom::PageHandler:
  void FocusChanged(bool focused) override;
  void StartPlatformVoiceRecognition() override;
  void HandleLensButtonClick() override;
  void HandleFileUpload(bool is_image) override;
  void NavigateUrl(const GURL& url) override;
  void CloseLensOverlayFromWebUI(
      composebox::mojom::LensOverlayDismissalSource dismissal_source) override;
  void SetSmartTabSharingActive(bool active) override;
  void GetSmartTabSharingActive(
      GetSmartTabSharingActiveCallback callback) override;
  void NotifyComposeboxQuerySubmittedWithContext() override;
  void CanShowNextboxAnimation(
      CanShowNextboxAnimationCallback callback) override;
  void RecordNextboxAnimationImpression() override;
  void OnContextMenuOpened() override;

  // searchbox::mojom::PageHandler:
  // These are stubs required to implement composebox::mojom::PageHandlerFactory
  // which is shared, but they are not used by the extension composebox.
  void OnFocusChanged(bool focused) override;
  void QueryAutocomplete(int32_t query_id,
                         const std::u16string& input,
                         bool prevent_inline_autocomplete,
                         uint32_t cursor_position,
                         omnibox::SuggestInventory suggest_inventory,
                         bool is_on_focus) override;
  void StopAutocomplete(bool clear_result) override;
  void OpenAutocompleteMatch(uint8_t line,
                             const GURL& url,
                             bool are_matches_showing,
                             uint8_t mouse_button,
                             searchbox::mojom::ActionModifiersPtr modifiers,
                             bool via_keyboard) override;
  void SetSmartComposeStats(
      searchbox::mojom::SmartComposeStatsPtr smart_compose_stats) override;
  void SetPopupSelection(
      searchbox::mojom::OmniboxPopupSelectionPtr selection) override;
  void SetInputMethod(searchbox::mojom::InputMethod input_method) override;
  void OpenPopupSelection(uint32_t result_sequence_id,
                          searchbox::mojom::OmniboxPopupSelectionPtr selection,
                          WindowOpenDisposition disposition) override;
  void OnNavigationLikely(
      uint8_t line,
      const GURL& url,
      omnibox::mojom::NavigationPredictor navigation_predictor) override;
  void DeleteAutocompleteMatch(uint8_t line, const GURL& url) override;
  void ActivateKeyword(uint8_t line,
                       const GURL& url,
                       base::TimeTicks match_selection_timestamp,
                       bool is_mouse_event) override;
  void ExecuteAction(uint8_t line,
                     uint8_t action_index,
                     const GURL& url,
                     base::TimeTicks match_selection_timestamp,
                     uint8_t mouse_button,
                     bool alt_key,
                     bool ctrl_key,
                     bool meta_key,
                     bool shift_key) override;
  void GetPlaceholderConfig(GetPlaceholderConfigCallback callback) override;
  void GetRecentTabs(GetRecentTabsCallback callback) override;
  void GetTabPreview(int32_t tab_id, GetTabPreviewCallback callback) override;
  void WaitForTabFaviconLoad(int32_t tab_id,
                             WaitForTabFaviconLoadCallback callback) override;
  void GetInputState(GetInputStateCallback callback) override;
  void NotifySessionStarted() override;
  void NotifySessionAbandoned() override;
  void AddFileContext(searchbox::mojom::SelectedFileInfoPtr file_info,
                      mojo_base::BigBuffer file_bytes,
                      AddFileContextCallback callback) override;
  void AddTabContext(int32_t tab_id,
                     bool delay_upload,
                     AddTabContextCallback callback) override;
  void DeleteContext(const base::UnguessableToken& file_token,
                     bool from_automatic_chip) override;
  void DeleteTabContext(int32_t tab_id) override;
  void ClearFiles(bool should_block_auto_suggested_tabs) override;
  void SubmitQuery(const std::string& query_text,
                   uint8_t mouse_button,
                   bool alt_key,
                   bool ctrl_key,
                   bool meta_key,
                   bool shift_key,
                   bool is_voice_search) override;
  void OpenLensSearch() override;
  void SetActiveToolMode(omnibox::ToolMode tool) override;
  void RecordToolSelectionAction(omnibox::ToolMode tool) override;
  void SetActiveModelMode(omnibox::ModelMode model) override;
  void RecordModelSelectionAction(omnibox::ModelMode model) override;
  void ActivateMetricsFunnel(const std::string& funnel_name) override;
  void GetDriveDisclaimerStatus(
      GetDriveDisclaimerStatusCallback callback) override;
  void OnDriveDisclaimerAccepted() override;
  void OnDriveUploadClicked(OnDriveUploadClickedCallback callback) override;
  void OpenProfilePicker() override;
  void GetPageClassification(GetPageClassificationCallback callback) override;
  void OnThumbnailRemoved() override;

 private:
  friend class content::DocumentUserData<ContextualTasksExtensionHandler>;
  explicit ContextualTasksExtensionHandler(content::RenderFrameHost* rfh);
  DOCUMENT_USER_DATA_KEY_DECL();

  mojo::Receiver<composebox::mojom::PageHandlerFactory>
      composebox_factory_receiver_{this};
  mojo::Receiver<composebox::mojom::PageHandler> composebox_handler_receiver_{
      this};
  mojo::Receiver<searchbox::mojom::PageHandler> searchbox_handler_receiver_{
      this};

  mojo::Remote<searchbox::mojom::Page> searchbox_page_;
};

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EXTENSION_HANDLER_H_
