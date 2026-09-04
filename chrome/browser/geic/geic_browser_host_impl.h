// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEIC_GEIC_BROWSER_HOST_IMPL_H_
#define CHROME_BROWSER_GEIC_GEIC_BROWSER_HOST_IMPL_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/geic/geic.mojom.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class BrowserTabStripTracker;
class BrowserWindowInterface;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace geic {

class TabContextExtractionRunner;

// Returns true if `wc` is eligible to be shared as tab context with GEiC.
// Uses an allowlist approach based on SchemeIsHTTPOrHTTPS and explicitly
// forbids reading another privileged WebContents.
bool IsTabValidForSharing(content::WebContents* wc);

// Classification for why a browser tab cannot be shared as context.
enum class RejectionKind {
  kNone,
  kProfileInvalid,   // null or off-the-record
  kNoBrowser,        // no active browser, or profile mismatch
  kNoActiveTab,      // no active tab in browser
  kTabNotShareable,  // privileged, or scheme not on the allowlist
};

// Implements the GEiC Mojo GeicBrowserHost API.
class GeicBrowserHostImpl : public mojom::GeicBrowserHost,
                            public TabStripModelObserver,
                            public BrowserTabStripTrackerDelegate {
 public:
  // Returns the active tab's WebContents only if it passes every sharing gate,
  // plus its metadata. Both are null when any gate fails, with `rejection`
  // indicating the failure kind.
  //
  // Async lifetime invariants:
  // - `contents` is used synchronously when initiating parallel extraction and
  //   is never passed across an async boundary. Resumption callbacks re-derive
  //   the active tab via GetValidatedActiveTab() and verify that the bound
  //   WeakDocumentPtr still resolves to the active tab's primary main frame
  //   rather than reusing earlier WebContents pointers.
  // - All upstream async APIs guarantee that their completion callbacks
  //   will always run even if the underlying tab/frame is destroyed:
  //   1) content_extraction::GetInnerText wraps its reply in
  //      mojo::WrapCallbackWithDefaultInvokeIfNotRun(..., nullptr).
  //   2) optimization_guide::GetAIPageContent uses timeout helpers and
  //      mojo::WrapCallbackWithDefaultInvokeIfNotRun.
  //   3) RenderWidgetHostView::CopyFromSurface guarantees callback execution
  //      (with bitmap drawsNothing() on failure / surface teardown).
  // - This ensures the Mojo response callback is never dropped unrun (which
  //   would trigger InterfaceEndpointClient::RaiseError() and disconnect the
  //   client pipe). If the tab closes mid-extraction, upstream returns a null
  //   or empty result, and our resumption check safely returns
  //   GetTabContextError::kNavigationInProgress.
  struct ValidatedActiveTab {
    raw_ptr<content::WebContents> contents = nullptr;
    mojom::TabMetadataPtr metadata;
    RejectionKind rejection = RejectionKind::kNone;
  };

  explicit GeicBrowserHostImpl(tabs::TabInterface* tab);
  GeicBrowserHostImpl(const GeicBrowserHostImpl&) = delete;
  GeicBrowserHostImpl& operator=(const GeicBrowserHostImpl&) = delete;
  ~GeicBrowserHostImpl() override;

  void BindBrowserHost(mojo::PendingReceiver<mojom::GeicBrowserHost> receiver);

  // mojom::GeicBrowserHost:
  void RegisterClient(mojo::PendingRemote<mojom::GeicClient> client,
                      RegisterClientCallback callback) override;
  void GetFocusedTab(GetFocusedTabCallback callback) override;
  void GetContextFromFocusedTab(
      mojom::TabContextOptionsPtr options,
      GetContextFromFocusedTabCallback callback) override;
  void OpenSignInTab(const GURL& signin_url) override;
  void CloseSignInTab(CloseSignInTabCallback callback) override;
  void ClosePanel() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabChangedAt(tabs::TabInterface* tab,
                      TabChangeType change_type) override;

  // BrowserTabStripTrackerDelegate:
  bool ShouldTrackBrowser(BrowserWindowInterface* browser) override;

  // Pushes a focused tab change notification to the client remote.
  void NotifyFocusedTabChanged(mojom::FocusedTabDataPtr data);

  // Accessor that re-derives and validates the active tab.
  ValidatedActiveTab GetValidatedActiveTab();

  // Returns the current focused tab data (either TabMetadata or
  // NoFocusedTabData with reason).
  mojom::FocusedTabDataPtr GetCurrentFocusedTabData();

  base::WeakPtr<GeicBrowserHostImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  TabContextExtractionRunner* tab_context_runner_for_testing() {
    return tab_context_runner_.get();
  }

 private:
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);
  void ResetTabContextRunner();

  raw_ptr<tabs::TabInterface> tab_ = nullptr;
  const raw_ptr<Profile> profile_ = nullptr;
  base::CallbackListSubscription will_detach_subscription_;
  std::unique_ptr<BrowserTabStripTracker> tab_strip_tracker_;
  base::WeakPtr<tabs::TabInterface> original_tab_;
  base::WeakPtr<content::WebContents> signin_web_contents_;
  bool has_opened_signin_tab_ = false;
  std::unique_ptr<TabContextExtractionRunner> tab_context_runner_;
  mojo::Receiver<mojom::GeicBrowserHost> receiver_{this};
  mojo::Remote<mojom::GeicClient> client_remote_;
  base::WeakPtrFactory<GeicBrowserHostImpl> weak_ptr_factory_{this};
};

}  // namespace geic

#endif  // CHROME_BROWSER_GEIC_GEIC_BROWSER_HOST_IMPL_H_
