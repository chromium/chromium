// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/selection/selection_overlay.mojom.h"
#include "chrome/browser/selection/mojom/action.mojom-forward.h"
#include "chrome/browser/selection/suggestion_service.h"
#include "chrome/browser/ui/lens/overlay_base_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "mojo/public/cpp/bindings/generic_pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/gfx/geometry/rect.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace views {
class WebView;
}

class BrowserWindowInterface;

namespace glic {

class FocusedTabData;
class GlicSharingManagerInternal;

class SelectionOverlayController
    : public OverlayBaseController,
      public selection::SelectionOverlayPageHandler,
      public TabStripModelObserver {
 public:
  enum class CloseReason {
    kOther,
    kCloseButton,
    kEscapeKeyPress,
  };

  SelectionOverlayController(tabs::TabInterface* tab,
                             PrefService* pref_service);
  ~SelectionOverlayController() override;

  DECLARE_USER_DATA(SelectionOverlayController);

  // A simple utility that gets the SelectionOverlayController TabFeature
  // set by the embedding tab of a overlay WebUI hosted in
  // `overlay_web_contents`. May return nullptr if no SelectionOverlayController
  // TabFeature is associated with `overlay_web_contents`.
  static SelectionOverlayController* FromOverlayWebContents(
      content::WebContents* overlay_web_contents);

  // A simple utility that gets the SelectionOverlayController TabFeature
  // set by the instances of WebContents associated with a tab. May return
  // nullptr if no SelectionOverlayController TabFeature is associated with
  // `tab_web_contents`.
  static SelectionOverlayController* FromTabWebContents(
      content::WebContents* tab_web_contents);

  // Returns the main frame of the overlay WebUI, or null if there's no
  // overlay WebUI.
  content::RenderFrameHost* GetOverlayMainFrame() const;

  size_t GetSelectedRegionCount() const {
    return session_->selected_regions.size();
  }
  std::vector<int> GetPolylineCounts() const;

  // This method is used to set up communication between this instance and the
  // overlay WebUI. This is called by the WebUIController when the WebUI is
  // executing javascript and ready to bind.
  void BindOverlay(
      mojo::PendingReceiver<selection::SelectionOverlayPageHandler> receiver,
      mojo::PendingRemote<selection::SelectionOverlayPage> page);

  // Bind the legacy IPC endpoint. See the comment on
  // `Session::capture_region_observer`.
  void BindCaptureRegionObserver(
      mojo::PendingRemote<mojom::CaptureRegionObserver> observer);
  static void CaptureRegion(
      tabs::TabInterface* tab,
      GlicSharingManagerInternal& sharing_manager,
      mojo::PendingRemote<mojom::CaptureRegionObserver> observer,
      mojom::TabContextOptionsPtr options);

  // Returns whether a new overlay session can start on the current tab,
  // applying to every entry point.
  bool CanStartSession();

  void Show(mojom::TabContextOptionsPtr options);
  // Shows the overlay with a region pre-selected around `selection_bounds`,
  // which is in screen coordinates.
  void ShowWithSelection(content::RenderFrameHost* selected_frame,
                         const gfx::Rect& selection_bounds,
                         selection::InteractionOptionsPtr interaction_options);
  void Close(CloseReason reason = CloseReason::kOther);

  using OverlayClosedCallback = base::OnceCallback<void(CloseReason)>;
  base::CallbackListSubscription RegisterOverlayClosedCallback(
      OverlayClosedCallback callback);

  // `selection::SelectionOverlayPageHandler`:
  void DeleteRegion(const base::UnguessableToken& id,
                    bool is_using_keyboard) override;

  using SuggestedActionsCallback = base::RepeatingCallback<void(
      const std::vector<selection::SuggestedActionPtr>&)>;
  void GetSuggestedActionsForTesting(SuggestedActionsCallback callback);

 private:
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);
  void WillDetach(tabs::TabInterface* tab,
                  tabs::TabInterface::DetachReason reason);
  void TabDeactivated(tabs::TabInterface* tab);
  void OnFocusedTabChanged(const FocusedTabData& tab_data);
  // This is the same event that drives `OnFocusedTabChanged()`, but it also
  // fires when no Glic instance is active.
  void OnActiveTabChanged(BrowserWindowInterface* window);
  void UpdateForTabVisibility();
  void ObserveActiveTabChanges();
  // Subscribes to the Glic side panel state for `tab_` for the length of the
  // session, and forwards changes to the overlay.
  void ObserveSidePanelState();
  void OnSidePanelStateChanged(GlicSidePanelCoordinator::State state);
  // What `SelectionOverlayPage::SetSidePanelOpen()` sends: the panel is
  // showing, or backgrounded and comes back with `tab_`.
  bool IsSidePanelOpen() const;
  // Called when the overlay's WebView takes focus, e.g. when the user clicks
  // on it. In a split view the overlay can be rendered over the inactive tab,
  // in which case `tab_` needs to be activated.
  void OnOverlayWebViewFocused(views::WebView* web_view);

  void InitializeOverlay();

  // `TabStripModelObserver`:
  void OnSplitTabChanged(const SplitTabChange& change) override;

  // OverlayBaseController overrides:
  void CloseUI() override;
  void RequestSyncClose(DismissalSource dismissal_source) override;
  void StartScreenshotFlow() override;
  void NotifyOverlayClosing() override;
  bool IsResultsSidePanelShowing() override;
  GURL GetInitialURL() override;
  void NotifyIsOverlayShowing(bool is_showing) override {}
  int GetToolResourceId() override;
  ui::ElementIdentifier GetViewContainerId() const override;
  SidePanelType GetSidePanelType() override;
  bool ShouldCloseSidePanel() override;
  bool ShouldShowPreselectionBubble() override;
  bool UseOverlayBlur() override;
  void NotifyPageNavigated() override;
  void NotifyTabForegrounded() override;
  void NotifyTabWillEnterBackground() override;
  PreselectionUIConfig GetPreselectionBubbleConfig() override;
  bool IsOverlayViewShared() const override;
  void ShowPreselectionBubble() override;
  void TabForegrounded(tabs::TabInterface* tab) override;

  // `selection::SelectionOverlayPageHandler`:
  void DismissOverlay(selection::DismissOverlayReason reason) override;
  void AdjustRegion(selection::SelectedRegionPtr target,
                    bool is_using_keyboard) override;
  void ClosePreselectionBubble() override;
  void AddBackgroundBlur() override;
  void SetLiveBlur(bool enabled) override;
  void SubmitPrompt(const std::string& prompt) override;
  void GetSuggestedActions(
      mojo::PendingRemote<selection::SuggestedActionsListener> listener)
      override;
  void ExecuteSuggestedAction(
      const base::UnguessableToken& action_id,
      mojo::GenericPendingAssociatedReceiver channel) override;
  void OpenSettings() override;
  void OpenDogfoodFeedback() override;

 private:
  void OnScreenshotTaken(const SkBitmap& bitmap);
  void OnScreenshotRedacted(const SkBitmap& bitmap);
  void PageContextReady(
      base::expected<glic::mojom::GetContextResultPtr,
                     page_content_annotations::FetchPageContextErrorDetails>
          fetch_result);

  void SetScreenshot(const SkBitmap& screenshot, SkBitmap rgb_screenshot);

  // The selected regions that fit in the redacted screenshot, in canvas
  // pixels, in the form the Glic web client takes. Needs the redacted
  // screenshot.
  using CapturedRegions = std::vector<
      std::pair<base::UnguessableToken, glic::mojom::CapturedRegionPtr>>;
  CapturedRegions GetCapturedRegions() const;
  // Sends the selected regions to the overlay WebUI and to the tab's Glic
  // instance. Nothing goes out until the redacted screenshot is here, and
  // nothing goes to the panel until the page context is; until then the panel
  // send is pending and `SyncRegionsToPanelIfPending()` finishes it.
  void SyncRegions(bool should_focus_panel);
  void SyncRegionsToPanelIfPending();
  void SendRegionsToPanel(CapturedRegions captured_regions,
                          bool should_focus_panel);

  struct SelectedRegionData {
    explicit SelectedRegionData(selection::SelectedRegionPtr region);
    SelectedRegionData(SelectedRegionData&&);
    SelectedRegionData& operator=(SelectedRegionData&&);
    ~SelectedRegionData();

    selection::SelectedRegionPtr region;
    std::optional<std::u16string> selected_text;
    std::optional<std::u16string> text_surrounding_selection;
    bool waiting_for_surrounding_text = false;
    std::vector<std::pair<base::UnguessableToken,
                          std::unique_ptr<::selection::Suggestion>>>
        suggestions;
    bool suggestions_requested = false;
    bool suggestions_complete = false;
    // The generation of the region. Incremented each time the region is
    // adjusted, and used to invalidate old suggestions.
    uint64_t generation = 0;
  };

  void Reset();
  void OnTextSurroundingSelectionAvailable(
      const base::UnguessableToken& region_id,
      uint64_t generation,
      const std::u16string& content,
      uint32_t start_offset,
      uint32_t end_offset);
  void GetSuggestedActionsImpl(SuggestedActionsCallback callback);
  // Callers must check `CanStartSession()` before staging any session state.
  void ShowImpl(mojom::TabContextOptionsPtr options);
  // Replaces any existing selected regions with a single region created from
  // `selection_bounds`, in screen coordinates. If `selected_frame` is live,
  // also requests the text surrounding its selection.
  void SetRegionFromBounds(content::RenderFrameHost* selected_frame,
                           const gfx::Rect& selection_bounds);
  void RequestNewSuggestions(SelectedRegionData& region_data);
  void OnSuggestionsReceived(
      const base::UnguessableToken& region_id,
      uint64_t generation,
      bool complete,
      std::vector<std::unique_ptr<::selection::Suggestion>> suggestions);
  glic::mojom::AdditionalContextPtr CreateAdditionalContext(
      CapturedRegions regions);

  // Everything that belongs to one overlay session. Staged by the entry points
  // before the session starts, and replaced with a fresh one by `Reset()` when
  // it ends, so nothing carries over to the next session.
  struct Session {
    explicit Session(selection::SelectionOverlayPageHandler* handler);
    ~Session();

    // Connections to and from the overlay WebUI. Only valid while
    // `OverlayBaseController::overlay_view_` is showing and the underlying
    // renderer is alive.
    mojo::Receiver<selection::SelectionOverlayPageHandler> receiver;
    mojo::Remote<selection::SelectionOverlayPage> page;

    // Legacy IPC that's used to signal the web client any browser side errors,
    // and used to dismiss the overlay from the web client. Only bound if the
    // overlay is invoked from the web client (web client UI, keyboard
    // shortcut). There are other invocations from outside the web client.
    // Don't use it to tell who started the session; that's `owner`.
    // TODO(b/452032491): Remove this once the old codepath is no longer used.
    mojo::Remote<mojom::CaptureRegionObserver> capture_region_observer;

    // Who started the session. Set by the entry point; a session the panel
    // adopts after opening mid-session becomes `kPanel` (b/577143587).
    enum class Owner {
      // Started without the web client: `Show()` / `ShowWithSelection()`,
      // from the hotkey, context menu or selection chip while the side panel
      // is closed. Handing off to Glic closes the overlay.
      kDirect,
      // Started by the side panel's web client through `CaptureRegion()`.
      // The overlay stays up after a hand-off, next to the panel.
      kPanel,
    };
    Owner owner = Owner::kDirect;

    bool screenshot_available = false;
    selection::InteractionOptionsPtr interaction_options =
        selection::InteractionOptions::New();
    // Whether `SyncRegions()` ran before the page context and the redacted
    // screenshot were here, and the panel still has to get the regions.
    bool panel_sync_pending = false;
    SkBitmap initial_rgb_screenshot;
    SkBitmap redacted_screenshot;
    mojom::TabContextResultPtr tab_context;
    mojom::TabContextOptionsPtr options;
    // Caches the user-selected regions and their associated suggestions. To be
    // rendered on top of `initial_rgb_screenshot`.
    base::flat_map<base::UnguessableToken, SelectedRegionData> selected_regions;
    std::optional<base::UnguessableToken> active_region_id;
    base::OneShotTimer surrounding_text_timer;
    mojo::Remote<selection::SuggestedActionsListener>
        suggested_actions_listener;
    SuggestedActionsCallback suggested_actions_callback_for_testing;
    // Subscription for `OverlayBaseController::overlay_web_view_` taking focus.
    // Scoped to the lifetime of that WebView.
    base::CallbackListSubscription overlay_web_view_focus_subscription;
    std::optional<CloseReason> close_reason;
    // Subscription to the active tab changes of the window that hosts `tab_`.
    base::CallbackListSubscription active_tab_subscription;
    // Subscription to the Glic side panel state for `tab_`, so the overlay
    // learns when the panel opens or closes.
    base::CallbackListSubscription side_panel_state_subscription;
  };
  // Never null.
  std::unique_ptr<Session> session_ = std::make_unique<Session>(this);

  std::unique_ptr<::selection::SuggestionTool> quick_answers_tool_;

  base::OnceCallbackList<void(CloseReason)> overlay_closed_callbacks_;

  ui::ScopedUnownedUserData<SelectionOverlayController>
      scoped_unowned_user_data_;

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  // For callbacks that belong to `session_`. Invalidated by `Reset()`.
  base::WeakPtrFactory<SelectionOverlayController> session_weak_factory_{this};
  // Must be the last member.
  base::WeakPtrFactory<SelectionOverlayController> weak_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_CONTROLLER_H_
