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
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/contextual_search/input_state_model.h"
#include "components/sessions/core/session_id.h"
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
struct ContextualInputData;
enum class LensOverlayDismissalSource;
class SearchToClientMessage_UpdateThreadContextLibrary;
}  // namespace lens

namespace omnibox {
struct InputState;
}  // namespace omnibox

namespace contextual_tasks {

class ContextualTasksWebContentsUserData
    : public content::WebContentsUserData<ContextualTasksWebContentsUserData> {
 public:
  enum class InjectedInputType {
    kContextLibrary,
    kLensChip,
  };

  struct ExtensionFrameInfo {
    ExtensionFrameInfo();
    ExtensionFrameInfo(const ExtensionFrameInfo&);
    ExtensionFrameInfo& operator=(const ExtensionFrameInfo&);
    ExtensionFrameInfo(ExtensionFrameInfo&&);
    ExtensionFrameInfo& operator=(ExtensionFrameInfo&&);
    ~ExtensionFrameInfo();

    raw_ptr<const void> handler_id = nullptr;
    bool is_page_bound = false;
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

  base::WeakPtr<contextual_search::InputStateModel> GetOrCreateInputStateModel(
      contextual_search::ContextualSearchSessionHandle& session_handle);
  base::WeakPtr<contextual_search::InputStateModel>
  GetOrCreateInputStateModel();

  static void UpdateInputStateModelIdentity(
      content::WebContents* web_contents,
      contextual_search::InputStateModel* input_state_model);

  const std::optional<base::Uuid>& pending_task_id() const {
    return pending_task_id_;
  }
  void set_pending_task_id(const std::optional<base::Uuid>& pending_task_id) {
    pending_task_id_ = pending_task_id;
  }

  const std::optional<base::Uuid>& task_id() const { return task_id_; }
  void SetTaskId(const base::Uuid& uuid);

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
  void OnDocumentConnected(content::RenderFrameHost* main_rfh);
  bool IsServiceWorkerPortConnected() const;
  bool IsHandshakeCompleteForTesting() const;
  const std::string& GetConnectedDocumentIdForTesting() const;

  // Parses and handles an incoming SearchToClientMessage. Returns true if the
  // message was a valid SearchToClientMessage with a recognized payload, or
  // false if the caller should fall back to legacy AimToClientMessage parsing.
  bool OnSearchMessageReceived(base::span<const uint8_t> message);
  void OnHandshakeComplete();

  // Sends a ClientToSearchMessage to the AIM page via the Service Worker Port
  // (if connected), queuing if the handshake is not yet complete, or falling
  // back to the primary bound extension iframe.
  void PostSearchMessage(const lens::ClientToSearchMessage& message);

  void SendInjectChromeInput(InjectedInputType type, bool is_active);
  void SendMountContextLibrary();
  void UpdateContextLibraryInputState();
  void HandleOnSubmitQueryRequest();
  void HandleOpenLinkInSidePanelMode(std::string_view url);
  // Syncs this thread's context library from AIM Search Web's tab history
  // into the `ContextualTasksService` for the current task.
  void HandleThreadContextLibraryUpdateFromAim(
      const lens::SearchToClientMessage_UpdateThreadContextLibrary& message);

  void OnLensThumbnailCreated(const std::string& thumbnail_uri);
  void RemoveLensCrop();

  contextual_search::ContextualSearchSessionHandle*
  GetOrCreateContextualSessionHandle();
  std::optional<base::UnguessableToken> GetLensOverlayToken();
#if !BUILDFLAG(IS_ANDROID)
  LensSearchController* GetLensSearchController() const;
  void SetTabContextSnapshot(
      const base::UnguessableToken& context_token,
      std::unique_ptr<lens::ContextualInputData> page_content_data);
  void ClearTabContextSnapshotIfMatching(const base::UnguessableToken& token);
#endif
  void UploadSnapshotTabContextIfPresent();
  void DoSubmitQueryCleanup();
  void CloseLensAsync(lens::LensOverlayDismissalSource dismissal_source);

  std::vector<contextual_search::TabInfo> GetSelectedTabs();
  void DeleteContext(const base::UnguessableToken& file_token);
  void DeleteTabContext(int32_t tab_id);
  void ClearFiles();

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

  base::WeakPtr<ContextualTasksWebContentsUserData> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void RecordTabIdMapping(int32_t tab_handle_id, int32_t session_tab_id);
  std::optional<int32_t> GetSessionTabIdForTabHandle(
      int32_t tab_handle_id) const;
  std::optional<int32_t> GetTabHandleForSessionTabId(
      int32_t session_tab_id) const;

 private:
  struct PageSearchState;

  explicit ContextualTasksWebContentsUserData(content::WebContents* contents);
  friend class content::WebContentsUserData<ContextualTasksWebContentsUserData>;

  void SubscribeToInputStateModel(
      base::WeakPtr<contextual_search::InputStateModel> model);
  void OnInputStateChanged(const omnibox::InputState& state);
  void RecordTimeToHandshakeComplete();
  void AppendTabContextsToOnSubmitQueryResponse(
      lens::ClientToSearchMessage* response_message,
      contextual_search::ContextualSearchSessionHandle* session_handle,
      const std::optional<base::UnguessableToken>& overlay_token);
  std::optional<lens::AddedContext> GetLensAddedContext();
  void DispatchSerializedSearchMessage(
      const std::vector<uint8_t>& message_bytes);
  PageSearchState& GetPrimaryPageSearchState();
  const PageSearchState* GetPrimaryPageSearchState() const;

  base::flat_map<base::UnguessableToken,
                 std::unique_ptr<contextual_search::InputStateModel>>
      input_state_models_;
  base::WeakPtr<contextual_search::InputStateModel> last_active_model_;
  base::WeakPtr<contextual_search::InputStateModel> subscribed_model_;
  base::CallbackListSubscription input_state_subscription_;

  // A pending task associated with this web contents.
  std::optional<base::Uuid> pending_task_id_;
  std::optional<base::Uuid> task_id_;

  std::vector<ExtensionFrameInfo> extension_frames_;

  base::TimeTicks last_handled_submit_interaction_time_;

#if !BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/568013317): Remove. Delayed tabs should be owned by the
  // session handle and uploaded at submit via QueryContextualizer, like
  // ContextualTasksComposeboxHandler.
  std::optional<std::pair<base::UnguessableToken,
                          std::unique_ptr<lens::ContextualInputData>>>
      tab_context_snapshot_;
#endif

  // Maps TabHandle raw values to SessionID values so closed tabs can still be
  // resolved when deleting tab context or removing underlines.
  base::flat_map<int32_t, int32_t> tab_handle_to_session_id_;

  SearchMessageDispatcherForTesting search_message_dispatcher_for_testing_;

  base::WeakPtrFactory<ContextualTasksWebContentsUserData> weak_ptr_factory_{
      this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_WEB_CONTENTS_USER_DATA_H_
