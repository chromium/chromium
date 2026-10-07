// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_SELECTION_OBSERVER_H_
#define CHROME_BROWSER_GLIC_GLIC_SELECTION_OBSERVER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/browser_ui/glic_selection_widget_controller_delegate.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/selection/shake_trigger.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_observer.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/gfx/geometry/rect.h"

namespace content {
class Page;
class RenderFrameHost;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

class BrowserWindowInterface;

namespace optimization_guide {
class PageContextEligibilityObserver;
class PageContextEligibility;
}  // namespace optimization_guide

namespace glic {

class GlicKeyedService;
class GlicSelectionWidgetController;

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
    kExternal,  // Click outside, focus change, scroll, resize, navigation, or
                // ESC key.
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

  bool has_sent_selection_context() const {
    return has_sent_selection_context_;
  }

  // Dismisses the selection UI.
  // Virtual for testing.
  virtual void DismissUI(DismissReason reason);

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

  // Returns true if Glic panel is showing for the current browser.
  // Virtual for testing.
  virtual bool IsPanelShowing(tabs::TabInterface* tab_interface,
                              BrowserWindowInterface* bwi);

  // Sends the selection context to the Glic panel.
  // Virtual for testing.
  virtual void SendAdditionalContextToPanel(
      tabs::TabInterface* tab_interface,
      const std::u16string& selected_text);

  // Shows the selection widget.
  // Virtual for testing.
  virtual void ShowSelectionAffordance(const std::u16string& selected_text);

  // Called when the page context eligibility changes.
  // Virtual for testing.
  virtual void OnPageContextEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus status);

  bool IsPageContextEligible() const;

  ::optimization_guide::PageContextEligibilityObserver* page_context_tracker() {
    return page_context_tracker_.get();
  }

 private:
  friend class GlicSelectionObserverTest;

  void ProcessPendingSelection();
  void ResetPendingSelection();
  void ProcessInputEvent(std::unique_ptr<blink::WebInputEvent> event);

  void OnGlobalPanelShowHide();

  void CreatePageContextEligibilityAPI(std::string account);
  void OnPageContextEligibilityAPILoaded(
      std::string account,
      optimization_guide::PageContextEligibility* page_context_eligibility);

  void ResetSelectionState();

  raw_ptr<GlicKeyedService> glic_keyed_service_;
  base::CallbackListSubscription panel_state_subscription_;
  std::u16string last_selected_text_;

  // The text of the last selection that was ignored due to rate limiting.
  std::optional<std::u16string> pending_selection_text_;

  std::optional<content::GlobalRenderFrameHostToken>
      last_selection_frame_token_;

  base::flat_set<content::GlobalRenderFrameHostToken> observed_frames_;

  // True if selection was initiated via keyboard shortcuts. Ensures KeyUp
  // events only trigger processing for relevant selection actions.
  bool is_key_selection_ = false;
  int bounds_retry_count_ = 0;

  // True if the selection context was sent to the Glic panel, so we know to
  // clear it if the selection becomes empty while the panel remains open.
  bool has_sent_selection_context_ = false;
  // True during active user selection (mouse drag or key hold) to defer UI
  // updates until the input event completes.
  bool is_selecting_ = false;

  std::unique_ptr<ShakeTrigger> shake_trigger_;

  std::unique_ptr<GlicSelectionWidgetController> widget_controller_;

  base::CallbackListSubscription page_context_eligibility_subscription_;
  std::unique_ptr<::optimization_guide::PageContextEligibilityObserver>
      page_context_tracker_;
  std::unique_ptr<ui::ScopedUnownedUserData<GlicSelectionObserver>>
      scoped_unowned_user_data_;
  base::WeakPtrFactory<GlicSelectionObserver> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_SELECTION_OBSERVER_H_
