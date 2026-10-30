// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_WEB_CONTENTS_USER_DATA_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_WEB_CONTENTS_USER_DATA_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_search/input_state_model.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

class BrowserWindowInterface;
class LensSearchController;

namespace contextual_search {
struct TabInfo;
}  // namespace contextual_search

namespace content {
class RenderFrameHost;
}  // namespace content

namespace lens {
class AddedContext;
class ClientToSearchMessage;
enum class LensOverlayDismissalSource;
enum class MimeType;
class SearchToClientMessage_UpdateThreadContextLibrary;
}  // namespace lens

namespace omnibox {
struct InputState;
}  // namespace omnibox

namespace contextual_tasks {

class ContextualTasksService;
class DesktopQueryContextualizerDelegate;
class QueryContextualizer;

class ContextualTasksWebContentsUserData
    : public content::WebContentsUserData<ContextualTasksWebContentsUserData>,
      public content::WebContentsObserver,
      public contextual_search::ContextualSearchContextController::
          ContextUploadStatusObserver,
      public ContextualTasksService::Observer {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(ContextualTasksThreadContextLibraryUpdateOutcome)
  enum class ThreadContextLibraryUpdateOutcome {
    kSuccessWithTaskId = 0,
    kSuccessWithoutTaskId = 1,
    kFeatureDisabled = 2,
    kNoSessionHandle = 3,
    kEmptyConvertedContext = 4,
    kMaxValue = kEmptyConvertedContext,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_tasks/enums.xml:ContextualTasksThreadContextLibraryUpdateOutcome)

  struct ExtensionFrameInfo {
    ExtensionFrameInfo();
    ExtensionFrameInfo(const ExtensionFrameInfo&);
    ExtensionFrameInfo& operator=(const ExtensionFrameInfo&);
    ExtensionFrameInfo(ExtensionFrameInfo&&);
    ExtensionFrameInfo& operator=(ExtensionFrameInfo&&);
    ~ExtensionFrameInfo();

    raw_ptr<const void> handler_id = nullptr;
    bool is_page_bound = false;
    bool is_handshake_complete = false;
    base::RepeatingCallback<void(const lens::ClientToSearchMessage&)>
        post_search_message_cb;
    base::RepeatingClosure on_handshake_complete_cb;
    base::RepeatingCallback<void(const GURL&)> on_lens_crop_updated_cb;
  };

  ~ContextualTasksWebContentsUserData() override;

  base::WeakPtr<contextual_search::InputStateModel> input_state_model() {
    return last_active_model_;
  }
  void set_input_state_model(
      std::unique_ptr<contextual_search::InputStateModel> input_state_model);
  static void UpdateInputStateModelIdentity(
      content::WebContents* web_contents,
      contextual_search::InputStateModel* input_state_model);

  base::WeakPtr<contextual_search::InputStateModel> GetOrCreateInputStateModel(
      contextual_search::ContextualSearchSessionHandle& session_handle);
  base::WeakPtr<contextual_search::InputStateModel>
  GetOrCreateInputStateModel();

  const std::optional<base::Uuid>& pending_task_id() const {
    return pending_task_id_;
  }
  void set_pending_task_id(const std::optional<base::Uuid>& pending_task_id) {
    pending_task_id_ = pending_task_id;
  }

  const std::optional<base::Uuid>& task_id() const { return task_id_; }
  void SetTaskId(std::optional<base::Uuid> task_id);
  void OnTaskChanged();
  // Returns `task_id_` if valid, else the task of this WebContents'
  // `ContextualSearchWebContentsHelper` (e.g. the side panel's task).
  // `task_id_` is set for full-tab AI Mode (see
  // `ContextualTasksTabNavigationObserver`).
  std::optional<base::Uuid> GetEffectiveTaskId() const;

  // Registers `callback` to run when the AI Mode page reports zero state
  // (`OnPageChanged`), before the input state is updated for it. See
  // `ContextualTasksPanelThreadObserver`.
  base::CallbackListSubscription RegisterZeroStateCallback(
      base::RepeatingClosure callback);

  void RegisterExtensionFrame(const void* handler_id);
  void UpdateExtensionFrameBound(
      const void* handler_id,
      bool is_page_bound,
      base::RepeatingCallback<void(const lens::ClientToSearchMessage&)>
          post_search_message_cb,
      base::RepeatingClosure on_handshake_complete_cb,
      base::RepeatingCallback<void(const GURL&)> on_lens_crop_updated_cb);
  void UnregisterExtensionFrame(const void* handler_id);
  bool HasBoundExtensionFrame() const;

  // Service Worker Port connection and search communication lifecycle.
  using ServiceWorkerPostMessageCallback =
      base::RepeatingCallback<void(const std::vector<uint8_t>&)>;
  void OnDocumentConnected(
      content::RenderFrameHost* main_rfh,
      ServiceWorkerPostMessageCallback post_message_cb = base::NullCallback());
  bool IsServiceWorkerPortConnected() const;
  bool IsHandshakeCompleteForTesting() const;
  const std::string& GetConnectedDocumentIdForTesting() const;

  // Parses and handles an incoming SearchToClientMessage. Returns true if the
  // message was a valid SearchToClientMessage with a recognized payload, or
  // false if the caller should fall back to legacy AimToClientMessage parsing.
  bool OnSearchMessageReceived(base::span<const uint8_t> message,
                               const void* source_handler_id = nullptr);
  void OnHandshakeComplete(const void* source_handler_id = nullptr);

  // Sends a ClientToSearchMessage to the AIM page via the Service Worker Port
  // (if connected), queuing if the handshake is not yet complete, or falling
  // back to the primary bound extension iframe.
  void PostSearchMessage(const lens::ClientToSearchMessage& message);

  void OnTabContextUploadStarted(const base::UnguessableToken& context_token);
  void OnTabContextRemoved(const base::UnguessableToken& context_token);

  // contextual_search::ContextualSearchContextController::
  //     ContextUploadStatusObserver:
  void OnContextUploadStatusChanged(
      const base::UnguessableToken& context_token,
      lens::MimeType mime_type,
      contextual_search::ContextUploadStatus context_upload_status,
      const std::optional<contextual_search::ContextUploadErrorType>&
          error_type) override;

  // ContextualTasksService::Observer:
  void OnTaskDisassociatedFromTab(const base::Uuid& task_id,
                                  SessionID tab_id) override;
  void OnWillBeDestroyed() override;

  // content::WebContentsObserver:
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  void HandleOnSubmitQueryRequest();
  void HandleOpenLinkInSidePanelMode(std::string_view url);
  // Syncs this thread's context library from AIM Search Web's tab history
  // into the `ContextualTasksService` for the current task.
  void HandleThreadContextLibraryUpdateFromAim(
      const lens::SearchToClientMessage_UpdateThreadContextLibrary& message);

  void OnLensThumbnailCreated(const std::string& thumbnail_uri);
  void RemoveLensCrop();

  contextual_search::ContextualSearchSessionHandle*
  GetExistingContextualSessionHandle();
  contextual_search::ContextualSearchSessionHandle*
  GetOrCreateContextualSessionHandle();
  std::optional<base::UnguessableToken> GetLensOverlayToken();
#if !BUILDFLAG(IS_ANDROID)
  LensSearchController* GetLensSearchController() const;
#endif
  void DoSubmitQueryCleanup();
  void CloseLensAsync(lens::LensOverlayDismissalSource dismissal_source);

  // Returns the tabs selected as context: the session handle's attached tabs
  // that are delayed tabs pending upload or whose context token is still
  // pending submission (in `GetUploadedContextTokens()`).
  std::vector<contextual_search::TabInfo> GetSelectedTabs();
  void DeleteContext(const base::UnguessableToken& file_token);
  void DeleteTabContext(int32_t tab_id);
  bool OnBrowserTabClosedOrNavigated(SessionID tab_session_id,
                                     std::optional<GURL> current_url,
                                     std::optional<std::string> current_title);
  void ClearFiles(bool should_block_auto_suggested_tabs = false);
  void ClearInputPlateState();

  // Syncs the zero-state active-tab delayed attachment with panel/tab state.
  void UpdateImplicitTabContext();
  // Unblocks `url` and clears `implicit_tab_` when manually attaching a tab.
  void OnManualTabContextAdded(const GURL& url, SessionID tab_session_id);
  std::optional<tabs::TabHandle> GetAutoSuggestedTabHandle() const;
  void RemoveImplicitTabContext();
  void PreservePreStagedContext();

  // Associates `tab_session_id` with the task, if any, so the task knows which
  // tabs have been attached as context.
  void AssociateTabWithTask(SessionID tab_session_id);
  // Asks the browser window's ActiveTaskContextProvider to recompute the
  // active task context (e.g. tab underlines) after context was removed.
  void RefreshActiveTaskContext();

  BrowserWindowInterface* GetBrowserWindowInterface() const;

  using SearchMessageDispatcherForTesting =
      base::RepeatingCallback<void(const lens::ClientToSearchMessage&)>;
  void SetSearchMessageDispatcherForTesting(
      SearchMessageDispatcherForTesting dispatcher) {
    search_message_dispatcher_for_testing_ = std::move(dispatcher);
  }
  // Recreates `query_contextualizer_` with `service`.
  void SetContextualTasksServiceForTesting(ContextualTasksService* service);

  base::WeakPtr<ContextualTasksWebContentsUserData> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void RecordTabIdMapping(int32_t tab_handle_id, int32_t session_tab_id);
  std::optional<int32_t> GetSessionTabIdForTabHandle(
      int32_t tab_handle_id) const;
  std::optional<int32_t> GetTabHandleForSessionTabId(
      int32_t session_tab_id) const;
  // Resolves `session_tab_id` to its `TabHandle` ID by checking existing tab ID
  // mappings and scanning open tabs in the browser window (recording the
  // mapping if a matching open tab is found). Returns `std::nullopt` if no
  // matching tab handle exists.
  std::optional<int32_t> ResolveAndRecordTabHandleForSessionTabId(
      int32_t session_tab_id);

  void AppendTabContextsToOnSubmitQueryResponseForTesting(
      lens::ClientToSearchMessage* response_message,
      contextual_search::ContextualSearchSessionHandle* session_handle,
      std::optional<base::UnguessableToken> overlay_token);

 private:
  enum class InjectedInputType {
    kContextLibrary,
    kLensChip,
  };
  enum class ContextState {
    kNone,
    kUploading,
    kReady,
  };
  struct PageSearchState;

  explicit ContextualTasksWebContentsUserData(content::WebContents* contents);
  friend class content::WebContentsUserData<ContextualTasksWebContentsUserData>;

  void SendInjectChromeInput(InjectedInputType type, bool is_active);
  void SendMountContextLibrary();
  void UpdateContextLibraryInputState();
  void SubscribeToInputStateModel(
      base::WeakPtr<contextual_search::InputStateModel> model);
  void OnInputStateChanged(const omnibox::InputState& state);
  void OnSessionChanged(
      contextual_search::ContextualSearchSessionHandle* session_handle);
  bool MaybeRefreshTabContextSubscription(
      contextual_search::ContextualSearchSessionHandle* session_handle);
  void OnTabContextUpdated(const contextual_search::TabContextState& state);
  void ObserveContextController(
      contextual_search::ContextualSearchContextController* controller);
  ContextState ComputeContextState();
  void UpdateContextState(bool force_send = false);
  std::vector<lens::AddedContext> ComputeSuggestInputContexts();
  void UpdateSuggestInputs();
  void RecordTimeToHandshakeComplete();
  // Returns the active tab to recontextualize on submit, unless the query has
  // a visual selection (`overlay_token`) or the tab was deselected. Like
  // `GetPendingDelayedTabs()`, returns `QueryContextualizer::TabId`s.
  std::vector<int32_t> GetTabsToRecontextualize(
      contextual_search::ContextualSearchSessionHandle& session_handle,
      const std::optional<base::UnguessableToken>& overlay_token);
  // Returns the delayed tabs in `session_handle` to upload on submit.
  std::vector<int32_t> GetPendingDelayedTabs(
      contextual_search::ContextualSearchSessionHandle& session_handle);
  // Posts the `on_submit_query_response` once `query_contextualizer_` has
  // uploaded the tabs for the submitted query.
  void OnSubmitQueryContextualized(
      std::optional<lens::AddedContext> lens_added_context,
      std::optional<base::UnguessableToken> overlay_token);
  void OnPageChanged(bool is_zero_state);
  void AppendTabContextsToOnSubmitQueryResponse(
      lens::ClientToSearchMessage* response_message,
      contextual_search::ContextualSearchSessionHandle* session_handle,
      const std::optional<base::UnguessableToken>& overlay_token);
  std::optional<lens::AddedContext> GetLensAddedContext();
  const ExtensionFrameInfo* FindTargetExtensionFrame() const;
  void DispatchSerializedSearchMessage(
      const std::vector<uint8_t>& message_bytes);
  PageSearchState& GetPrimaryPageSearchState();
  const PageSearchState* GetPrimaryPageSearchState() const;
  // Returns true if the session has tabs for the context library to show:
  // unsubmitted tabs, submitted (persisted) tabs, or tabs restored from the AIM
  // thread. Keep in sync with
  // `ContextualTasksExtensionHandler::SendTabContextToExtensionPage()`.
  bool HasContextLibraryTabs();

  struct ImplicitTab {
    base::UnguessableToken token;
    tabs::TabHandle handle;
  };
  std::optional<ImplicitTab> implicit_tab_;
  base::flat_set<GURL> dismissed_implicit_urls_;
  bool preserve_prestaged_context_ = false;
  base::RepeatingClosureList zero_state_callbacks_;

  base::flat_map<base::UnguessableToken,
                 std::unique_ptr<contextual_search::InputStateModel>>
      input_state_models_;
  base::WeakPtr<contextual_search::InputStateModel> last_active_model_;
  base::WeakPtr<contextual_search::InputStateModel> subscribed_model_;
  base::CallbackListSubscription input_state_subscription_;
  base::CallbackListSubscription session_changed_subscription_;
  base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
      subscribed_session_handle_;
  base::CallbackListSubscription tab_context_subscription_;
  base::WeakPtr<contextual_search::ContextualSearchContextController>
      observed_controller_;

  // A pending task associated with this web contents.
  std::optional<base::Uuid> pending_task_id_;
  std::optional<base::Uuid> task_id_;

  std::vector<ExtensionFrameInfo> extension_frames_;
  // TODO(crbug.com/573125119): Clean up per-frame handshake tracking and
  // routing fallback once Service Worker communication lands.
  raw_ptr<const void> last_active_search_handler_id_ = nullptr;
  base::flat_set<base::UnguessableToken> pending_context_uploads_;
  base::flat_set<base::UnguessableToken> failed_context_uploads_;

  base::TimeTicks last_handled_submit_interaction_time_;

  // Uploads delayed tabs and recontextualizes the active tab on submit.
  std::unique_ptr<DesktopQueryContextualizerDelegate>
      query_contextualizer_delegate_;
  std::unique_ptr<QueryContextualizer> query_contextualizer_;
  // Whether a submit is waiting for `query_contextualizer_`.
  bool submit_pending_ = false;

  // Maps TabHandle raw values to SessionID values so closed tabs can still be
  // resolved when deleting tab context or removing underlines.
  base::flat_map<int32_t, int32_t> tab_handle_to_session_id_;

  SearchMessageDispatcherForTesting search_message_dispatcher_for_testing_;

  base::ScopedObservation<ContextualTasksService,
                          ContextualTasksService::Observer>
      contextual_tasks_service_observation_{this};

  base::WeakPtrFactory<ContextualTasksWebContentsUserData> weak_ptr_factory_{
      this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_WEB_CONTENTS_USER_DATA_H_
