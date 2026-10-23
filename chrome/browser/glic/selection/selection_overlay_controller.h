// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_CONTROLLER_H_

#include <string>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/glic.mojom.h"
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
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"

namespace content {
class RenderFrameHost;
class WebContents;
}

namespace input {
struct NativeWebKeyboardEvent;
}

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

  size_t GetSelectedRegionCount() const { return selected_regions_.size(); }
  std::vector<int> GetPolylineCounts() const;

  // This method is used to set up communication between this instance and the
  // overlay WebUI. This is called by the WebUIController when the WebUI is
  // executing javascript and ready to bind.
  void BindOverlay(
      mojo::PendingReceiver<selection::SelectionOverlayPageHandler> receiver,
      mojo::PendingRemote<selection::SelectionOverlayPage> page);

  // Bind the legacy IPC endpoint. See the comment on
  // `capture_region_observer_`.
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
  // Called when the overlay's WebView takes focus, e.g. when the user clicks
  // on it. In a split view the overlay can be rendered over the inactive tab,
  // in which case `tab_` needs to be activated.
  void OnOverlayWebViewFocused(views::WebView* web_view);

  void InitializeOverlay();

  // `content::WebContentsDelegate`:
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;

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

 private:
  void OnScreenshotTaken(const SkBitmap& bitmap);
  void OnScreenshotRedacted(const SkBitmap& bitmap);
  void PageContextReady(
      base::expected<glic::mojom::GetContextResultPtr,
                     page_content_annotations::FetchPageContextErrorDetails>
          fetch_result);

  void SetScreenshot(const SkBitmap& screenshot, SkBitmap rgb_screenshot);

  // Render all the `selected_regions_` on top of `redacted_screenshot_`.
  void RenderRegions(bool should_focus_panel);
  // Renders the pre-selected regions once the overlay is bound and the page
  // context is ready. Called from each of those steps, so whichever finishes
  // last does the work.
  void RenderPendingRegions();

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
      std::vector<std::pair<base::UnguessableToken,
                            glic::mojom::CapturedRegionPtr>> regions);

  // Connections to and from the overlay WebUI. Only valid while
  // `OverlayBaseController::overlay_view_` is showing and the underlying
  // renderer is alive.
  mojo::Receiver<selection::SelectionOverlayPageHandler> receiver_{this};
  mojo::Remote<selection::SelectionOverlayPage> page_;

  // Legacy IPC that's used to signal the web client any browser side errors,
  // and used to dismiss the overlay from the web client. Only bound if the
  // overlay is invoked from the web client (web client UI, keyboard shortcut).
  // There are other invocations from outside the web client.
  // TODO(b/452032491): Remove this once the old codepath is no longer used.
  mojo::Remote<mojom::CaptureRegionObserver> capture_region_observer_;

  // Stateful members. They should be added to Reset().
  bool screenshot_available_ = false;
  selection::InteractionOptionsPtr interaction_options_ =
      selection::InteractionOptions::New();
  // Whether the regions pre-selected at the start of the session still need
  // to be sent to the web client.
  bool staged_region_needs_render_ = false;
  SkBitmap initial_rgb_screenshot_;
  SkBitmap redacted_screenshot_;
  mojom::TabContextResultPtr tab_context_;
  mojom::TabContextOptionsPtr options_;
  // Caches the user-selected regions and their associated suggestions. To be
  // rendered on top of `initial_screenshot_`.
  base::flat_map<base::UnguessableToken, SelectedRegionData> selected_regions_;
  std::optional<base::UnguessableToken> active_region_id_;
  base::OneShotTimer surrounding_text_timer_;
  mojo::Remote<selection::SuggestedActionsListener> suggested_actions_listener_;
  SuggestedActionsCallback suggested_actions_callback_for_testing_;
  // Subscription for `OverlayBaseController::overlay_web_view_` taking focus.
  // Scoped to the lifetime of that WebView.
  base::CallbackListSubscription overlay_web_view_focus_subscription_;
  std::unique_ptr<::selection::SuggestionTool> quick_answers_tool_;
  std::optional<CloseReason> close_reason_;

  base::OnceCallbackList<void(CloseReason)> overlay_closed_callbacks_;

  ui::ScopedUnownedUserData<SelectionOverlayController>
      scoped_unowned_user_data_;

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;
  // Subscription to the active tab changes of the window that hosts `tab_`.
  base::CallbackListSubscription active_tab_subscription_;

  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  // Must be the last member.
  base::WeakPtrFactory<SelectionOverlayController> weak_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_CONTROLLER_H_
