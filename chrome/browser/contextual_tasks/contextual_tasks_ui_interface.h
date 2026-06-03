// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_INTERFACE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_INTERFACE_H_

#include "base/observer_list.h"
#include "chrome/browser/contextual_tasks/task_info_delegate.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "content/public/browser/page_navigator.h"
#include "mojo/public/cpp/bindings/remote.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace contextual_search {
class ContextualSearchSessionHandle;
class InputStateModel;
}  // namespace contextual_search

namespace lens {
class ClientToAimMessage;
}  // namespace lens

namespace contextual_tasks {
class ContextualTasksComposeboxHandlerInterface;
namespace mojom {
class Page;
}  // namespace mojom

class ContextualTasksAutoSuggestionManager;

// An interface to interact with the Contextual Tasks WebUI (ContextualTasksUI)
// from the rest of the browser process.
class ContextualTasksUIInterface : public TaskInfoDelegate {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnInitComplete() {}
  };

  ~ContextualTasksUIInterface() override = default;

  // Returns the auto-suggestion manager.
  virtual ContextualTasksAutoSuggestionManager* GetAutoSuggestionManager() = 0;

  // Returns the Profile associated with this WebUI.
  virtual Profile* GetProfile() = 0;

  // UI states and commands.

  // Transfers an existing navigation to the page embedded in this WebUI. This
  // API will only accept navigations to the AI or search results pages.
  virtual void TransferNavigationToEmbeddedPage(
      content::OpenURLParams params) = 0;

  // Closes the side panel if it is currently showing this WebUI.
  virtual void CloseSidePanel() = 0;

  // Notifies the UI that the WebContents has moved between the side panel and
  // a tab.
  virtual void OnSidePanelStateChanged() = 0;

  // Called when the active tab has been changed (e.g. new page loaded or title
  // change). This is used to update the UI for both tab/side panel modes.
  // Note that a title can be updated while the page is loading, so this can
  // be called even when active tab has not changed.
  virtual void OnActiveTabContextStatusChanged() = 0;

  // Notifies the UI that the Lens overlay state has changed.
  virtual void OnLensOverlayStateChanged(
      bool is_showing,
      std::optional<lens::LensOverlayInvocationSource> invocation_source) = 0;

  // Returns whether the Lens overlay is currently showing.
  virtual bool IsLensOverlayShowing() const = 0;

  // Starts the platform's native voice recognition system.
  virtual void StartPlatformVoiceRecognition() = 0;

  // Called when the platform voice recognition completes with a result.
  virtual void OnVoiceTranscribed(const std::string& query) = 0;

  // Notifies the UI of the page context eligibility.
  virtual void OnPageContextEligibilityChecked(
      bool is_page_context_eligible) = 0;

  // Returns whether the active tab context suggestion is currently showing.
  virtual bool IsActiveTabContextSuggestionShowing() const = 0;

  // Returns whether the UI can be expanded to a full tab.
  virtual bool CanExpandToFullTab() const = 0;

  // Moves the UI associated with this WebUI to a new tab.
  virtual void MoveTaskUiToNewTab() = 0;

  // Notifies the UI that the expand button enabled state should be updated.
  virtual void UpdateExpandButtonEnabled(bool enabled) = 0;

  virtual GURL GetWebUiUrl() = 0;

  // Mojo & Session.

  // Sends a message to the <webview> guest. The WebUI is responsible for
  // taking the 'message' (a serialized lens.ClientToAimMessage protobuf) and
  // using the <webview> postMessage API to send it to the guest content.
  virtual void PostMessageToWebview(
      const lens::ClientToAimMessage& message) = 0;

  // Lazily creates and returns a reference to the owned contextual search
  // session handle.
  virtual contextual_search::ContextualSearchSessionHandle*
  GetOrCreateContextualSessionHandle() = 0;

  // Returns the Mojo remote used to communicate with the WebUI page.
  virtual mojo::Remote<contextual_tasks::mojom::Page>& GetPageRemote() = 0;

  // Fetches and assumes unique ownership of the pre-configured input state
  // model attached to the WebContents for the current task. Subsequent calls
  // for the same task will return nullptr.
  virtual std::unique_ptr<contextual_search::InputStateModel>
  TakeInputStateModel() = 0;

  // Fetches the restored tab IDs attached to the WebContents for the
  // current task.
  virtual std::vector<int32_t> GetRestoredTabIds() = 0;

  // Notifies the UI that restored tabs have been successfully fetched from the
  // database.
  void OnRestoredTabsFetched(
      std::vector<searchbox::mojom::TabInfoPtr> tabs) override = 0;

  // Registers the composebox handler with this UI.
  virtual void SetComposeboxHandler(
      ContextualTasksComposeboxHandlerInterface* handler) = 0;

  // Helpers.

  // Returns the URL of the page currently embedded in the WebUI's <webview>.
  virtual const GURL& GetInnerFrameUrl() const = 0;

  // Returns the WebContents of the embedded page, if it exists.
  virtual content::WebContents* GetInnerWebContents() const = 0;

  // Returns whether the web ui is initialized.
  virtual bool IsInitComplete() = 0;
  // A notification that the web ui is initialized.
  virtual void OnInitComplete() = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_INTERFACE_H_
