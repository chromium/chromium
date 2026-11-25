// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_SELECTION_OBSERVER_H_
#define CHROME_BROWSER_GLIC_GLIC_SELECTION_OBSERVER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/browser_ui/glic_selection_widget_controller_delegate.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/selection/shake_trigger.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace content {
class Page;
class RenderFrameHost;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

class GlicSelectionWidgetController;
class SelectionContextSender;

class GlicSelectionObserver
    : public content::WebContentsObserver,
      public content::RenderWidgetHost::InputEventObserver,
      public GlicSelectionWidgetControllerDelegate,
      public ShakeTriggerClient {
 public:
  DECLARE_USER_DATA(GlicSelectionObserver);

  enum class DismissReason {
    kActionTaken,  // User clicked Ask Gemini, Copy, or Copy Link.
    kCloseButton,  // User clicked the close button on the widget.
    kExternal,     // Click outside, focus change, scroll, resize, navigation,
                   // context menu, or ESC key.
  };

  enum class SelectionSource {
    kAutomatic,    // Triggered by WebContents text selection or input events.
    kContextMenu,  // Triggered by context menu invocation.
  };

  static GlicSelectionObserver* From(tabs::TabInterface* tab);

  explicit GlicSelectionObserver(content::WebContents* web_contents);
  ~GlicSelectionObserver() override;

  // `content::WebContentsObserver`:
  void OnTextSelectionChanged(content::RenderFrameHost* render_frame_host,
                              std::u16string_view selected_text) override;

  // `GlicSelectionWidgetControllerDelegate`:
  content::RenderFrameHost* GetSelectedFrame() const override;
  std::optional<gfx::Rect> GetCurrentSelectionBounds() const override;
  const std::u16string& GetSelectedText() const override;

  // Notifies the observer that text selection context was sent to the Glic
  // panel from the context menu entry point.
  void UpdateSelectionStateFromContextMenu(const std::u16string& selected_text);

  // Notifies the observer that the smart suggestion context menu was invoked
  // on `selected_text`, ensuring the selection is tracked (and sent to the
  // panel when open) even for selections in editable fields.
  void UpdateSelectionStateFromSmartSuggestionContextMenu(
      const std::u16string& selected_text);

  bool has_sent_selection_context() const;

  // Dismisses the selection UI.
  // Virtual for testing.
  virtual void DismissUI(DismissReason reason);

  bool IsShowingWidgetForTesting() const;

 protected:
  // `ShakeTriggerClient`:
  bool IsTextSelectionSharingEnabled() const override;
  bool IsSidePanelOpen() const override;

  // `content::WebContentsObserver`:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void PrimaryPageChanged(content::Page& page) override;
  void PrimaryMainFrameWasResized(bool width_changed) override;
  void OnWebContentsLostFocus(
      content::RenderWidgetHost* render_widget_host) override;

  // `content::RenderWidgetHost::InputEventObserver`:
  void OnInputEvent(
      const content::RenderWidgetHost& host,
      const blink::WebInputEvent& event,
      content::RenderWidgetHost::InputEventObserver::InputEventSource source)
      override;

  // Updates the Glic UI (widget or panel) with the selected text.
  // Virtual for testing.
  virtual void UpdateSelectionState(const std::u16string& text,
                                    bool is_pending_selection,
                                    SelectionSource source);

  // Returns true if the inline cue is enabled for the current profile.
  // Virtual for testing.
  virtual bool IsInlineCueEnabled() const;

  // Shows the selection widget.
  // Virtual for testing.
  virtual void ShowSelectionAffordance(const std::u16string& selected_text);

  // Closes the selection widget, if any.
  // Virtual for testing.
  virtual void CloseSelectionAffordance();

 private:
  friend class GlicSelectionObserverTest;

  void ProcessPendingSelection();
  void ResetPendingSelection();
  void ResetFollowUpSelection();
  void ProcessInputEvent(std::unique_ptr<blink::WebInputEvent> event);

  void OnGlobalPanelShowHide();
  void EnsureSidePanelStateSubscription();
  void OnSidePanelStateChanged(GlicSidePanelCoordinator::State state);
  SelectionContextSender* GetSelectionContextSender() const;

  void ResetSelectionState();
  bool IsQuickAnswersEligible() const;

  // Set in the constructor when the tab already has a sender: `TabFeatures`
  // creates the sender before the observer and destroys it after. Null when
  // the observer is re-created for a discard (the new contents isn't tied to
  // the tab yet) and in tests that attach a sender later; those look it up on
  // use.
  raw_ptr<SelectionContextSender> selection_context_sender_ = nullptr;
  base::CallbackListSubscription panel_state_subscription_;
  base::CallbackListSubscription side_panel_state_subscription_;
  GlicSidePanelCoordinator::State last_side_panel_state_ =
      GlicSidePanelCoordinator::State::kClosed;
  std::u16string last_selected_text_;
  gfx::Size last_web_contents_size_;

  // The text of the last selection that was ignored due to rate limiting.
  std::optional<std::u16string> pending_selection_text_;

  std::optional<content::GlobalRenderFrameHostToken>
      last_selection_frame_token_;

  base::flat_set<content::GlobalRenderFrameHostToken> observed_frames_;

  // True if selection was initiated via keyboard shortcuts. Ensures KeyUp
  // events only trigger processing for relevant selection actions.
  bool is_key_selection_ = false;
  int bounds_retry_count_ = 0;

  // True during active user selection (mouse drag or key hold) to defer UI
  // updates until the input event completes.
  bool is_selecting_ = false;

  // The latest selection, and the last one counted as a wiggle follow-up.
  std::optional<size_t> follow_up_selection_hash_;
  std::optional<size_t> counted_follow_up_selection_hash_;

  std::unique_ptr<ShakeTrigger> shake_trigger_;

  std::unique_ptr<GlicSelectionWidgetController> widget_controller_;

  std::unique_ptr<ui::ScopedUnownedUserData<GlicSelectionObserver>>
      scoped_unowned_user_data_;
  base::WeakPtrFactory<GlicSelectionObserver> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_SELECTION_OBSERVER_H_
