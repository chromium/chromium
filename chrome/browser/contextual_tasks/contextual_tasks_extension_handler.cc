// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_extension_handler.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

DOCUMENT_USER_DATA_KEY_IMPL(ContextualTasksExtensionHandler);

ContextualTasksExtensionHandler::ContextualTasksExtensionHandler(
    content::RenderFrameHost* rfh)
    : content::DocumentUserData<ContextualTasksExtensionHandler>(rfh) {}

ContextualTasksExtensionHandler::~ContextualTasksExtensionHandler() = default;

void ContextualTasksExtensionHandler::BindComposeboxFactory(
    mojo::PendingReceiver<composebox::mojom::PageHandlerFactory> receiver) {
  composebox_factory_receiver_.reset();
  composebox_factory_receiver_.Bind(std::move(receiver));
}

// composebox::mojom::PageHandlerFactory:
void ContextualTasksExtensionHandler::CreatePageHandler(
    mojo::PendingReceiver<composebox::mojom::PageHandler> receiver,
    mojo::PendingRemote<searchbox::mojom::Page> searchbox_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler> searchbox_handler) {
  composebox_handler_receiver_.reset();
  composebox_handler_receiver_.Bind(std::move(receiver));

  searchbox_page_.reset();
  searchbox_page_.Bind(std::move(searchbox_page));
  searchbox_handler_receiver_.reset();
  searchbox_handler_receiver_.Bind(std::move(searchbox_handler));
}

// composebox::mojom::PageHandler stubs:
void ContextualTasksExtensionHandler::FocusChanged(bool focused) {}
void ContextualTasksExtensionHandler::StartPlatformVoiceRecognition() {}
void ContextualTasksExtensionHandler::HandleLensButtonClick() {}
void ContextualTasksExtensionHandler::HandleFileUpload(bool is_image) {}
void ContextualTasksExtensionHandler::NavigateUrl(const GURL& url) {}
void ContextualTasksExtensionHandler::CloseLensOverlayFromWebUI(
    composebox::mojom::LensOverlayDismissalSource dismissal_source) {}
void ContextualTasksExtensionHandler::SetSmartTabSharingActive(bool active) {}
void ContextualTasksExtensionHandler::GetSmartTabSharingActive(
    GetSmartTabSharingActiveCallback callback) {
  std::move(callback).Run(false);
}
void ContextualTasksExtensionHandler::
    NotifyComposeboxQuerySubmittedWithContext() {}
void ContextualTasksExtensionHandler::CanShowNextboxAnimation(
    CanShowNextboxAnimationCallback callback) {
  std::move(callback).Run(false);
}
void ContextualTasksExtensionHandler::RecordNextboxAnimationImpression() {}
void ContextualTasksExtensionHandler::OnContextMenuOpened() {}

// searchbox::mojom::PageHandler stubs:
void ContextualTasksExtensionHandler::OnFocusChanged(bool focused) {}
void ContextualTasksExtensionHandler::QueryAutocomplete(
    int32_t query_id,
    const std::u16string& input,
    bool prevent_inline_autocomplete,
    uint32_t cursor_position,
    omnibox::SuggestInventory suggest_inventory,
    bool is_on_focus) {}
void ContextualTasksExtensionHandler::StopAutocomplete(bool clear_result) {}
void ContextualTasksExtensionHandler::OpenAutocompleteMatch(
    uint8_t line,
    const GURL& url,
    bool are_matches_showing,
    uint8_t mouse_button,
    searchbox::mojom::ActionModifiersPtr modifiers,
    bool via_keyboard) {
  NavigateUrl(url);
}
void ContextualTasksExtensionHandler::SetSmartComposeStats(
    searchbox::mojom::SmartComposeStatsPtr smart_compose_stats) {}
void ContextualTasksExtensionHandler::SetPopupSelection(
    searchbox::mojom::OmniboxPopupSelectionPtr selection) {}
void ContextualTasksExtensionHandler::SetInputMethod(
    searchbox::mojom::InputMethod input_method) {}
void ContextualTasksExtensionHandler::OpenPopupSelection(
    uint32_t result_sequence_id,
    searchbox::mojom::OmniboxPopupSelectionPtr selection,
    WindowOpenDisposition disposition) {}
void ContextualTasksExtensionHandler::OnNavigationLikely(
    uint8_t line,
    const GURL& url,
    omnibox::mojom::NavigationPredictor navigation_predictor) {}
void ContextualTasksExtensionHandler::DeleteAutocompleteMatch(uint8_t line,
                                                              const GURL& url) {
}
void ContextualTasksExtensionHandler::ActivateKeyword(
    uint8_t line,
    const GURL& url,
    base::TimeTicks match_selection_timestamp,
    bool is_mouse_event) {}
void ContextualTasksExtensionHandler::ExecuteAction(
    uint8_t line,
    uint8_t action_index,
    const GURL& url,
    base::TimeTicks match_selection_timestamp,
    uint8_t mouse_button,
    bool alt_key,
    bool ctrl_key,
    bool meta_key,
    bool shift_key) {}
void ContextualTasksExtensionHandler::GetCyclingPlaceholderConfig(
    GetCyclingPlaceholderConfigCallback callback) {
  std::move(callback).Run(nullptr);
}
void ContextualTasksExtensionHandler::GetRecentTabs(
    GetRecentTabsCallback callback) {
  std::move(callback).Run({});
}
void ContextualTasksExtensionHandler::GetTabPreview(
    int32_t tab_id,
    GetTabPreviewCallback callback) {
  std::move(callback).Run("");
}
void ContextualTasksExtensionHandler::WaitForTabFaviconLoad(
    int32_t tab_id,
    WaitForTabFaviconLoadCallback callback) {
  std::move(callback).Run(std::nullopt);
}
void ContextualTasksExtensionHandler::GetInputState(
    GetInputStateCallback callback) {
  std::move(callback).Run(std::nullopt);
}
void ContextualTasksExtensionHandler::NotifySessionStarted() {}
void ContextualTasksExtensionHandler::NotifySessionAbandoned() {}
void ContextualTasksExtensionHandler::AddFileContext(
    searchbox::mojom::SelectedFileInfoPtr file_info,
    mojo_base::BigBuffer file_bytes,
    AddFileContextCallback callback) {}
void ContextualTasksExtensionHandler::AddTabContext(
    int32_t tab_id,
    bool delay_upload,
    AddTabContextCallback callback) {}
void ContextualTasksExtensionHandler::DeleteContext(
    const base::UnguessableToken& file_token,
    bool from_automatic_chip) {}
void ContextualTasksExtensionHandler::DeleteTabContext(int32_t tab_id) {}
void ContextualTasksExtensionHandler::ClearFiles(
    bool should_block_auto_suggested_tabs) {}
void ContextualTasksExtensionHandler::SubmitQuery(const std::string& query_text,
                                                  uint8_t mouse_button,
                                                  bool alt_key,
                                                  bool ctrl_key,
                                                  bool meta_key,
                                                  bool shift_key,
                                                  bool is_voice_search) {}
void ContextualTasksExtensionHandler::OpenLensSearch() {}
void ContextualTasksExtensionHandler::SetActiveToolMode(
    omnibox::ToolMode tool) {}
void ContextualTasksExtensionHandler::RecordToolSelectionAction(
    omnibox::ToolMode tool) {}
void ContextualTasksExtensionHandler::SetActiveModelMode(
    omnibox::ModelMode model) {}
void ContextualTasksExtensionHandler::RecordModelSelectionAction(
    omnibox::ModelMode model) {}
void ContextualTasksExtensionHandler::ActivateMetricsFunnel(
    const std::string& funnel_name) {}
void ContextualTasksExtensionHandler::GetDriveDisclaimerStatus(
    GetDriveDisclaimerStatusCallback callback) {
  std::move(callback).Run(
      searchbox::mojom::DriveDisclaimerStatus::kNotAccepted);
}
void ContextualTasksExtensionHandler::OnDriveDisclaimerAccepted() {}
void ContextualTasksExtensionHandler::OnDriveUploadClicked(
    OnDriveUploadClickedCallback callback) {}
void ContextualTasksExtensionHandler::OpenProfilePicker() {}
void ContextualTasksExtensionHandler::GetPageClassification(
    GetPageClassificationCallback callback) {
  std::move(callback).Run("INVALID_SPEC");
}
void ContextualTasksExtensionHandler::OnThumbnailRemoved() {}
