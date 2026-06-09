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
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/host/host.h"
#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/link_to_text/link_to_text.mojom.h"
#include "ui/views/widget/widget.h"

namespace content {
class Page;
class RenderFrameHost;
}  // namespace content

namespace views {
class Widget;
}  // namespace views

class BrowserWindowInterface;

namespace glic {

enum class GlicNudgeActivity;

class GlicSelectionWidgetDelegate;
class GlicKeyedService;

class GlicSelectionObserver
    : public content::WebContentsObserver,
      public content::RenderWidgetHost::InputEventObserver {
 public:
  explicit GlicSelectionObserver(content::WebContents* web_contents);
  ~GlicSelectionObserver() override;

  void OnTextSelectionChanged(content::RenderFrameHost* render_frame_host,
                              std::u16string_view selected_text) override;

 protected:
  // Updates the Glic UI (nudge or panel) with the selected text.
  // Virtual for testing.
  virtual void UpdateSelectionState(const std::u16string& text,
                                    bool is_pending_selection);

  // Dismisses the selection UI (widget and/or nudge).
  // Virtual for testing.
  virtual void DismissUI(bool keep_nudge);

  // Returns true if the selection prompt is enabled for the current profile.
  virtual bool IsSelectionPromptEnabled() const;

  // Returns true if Glic panel is showing for the current browser.
  // Virtual for testing.
  virtual bool IsPanelShowing(tabs::TabInterface* tab_interface,
                              BrowserWindowInterface* bwi);

  // Sends the selection context to the Glic panel.
  // Virtual for testing.
  virtual void SendAdditionalContextToPanel(
      tabs::TabInterface* tab_interface,
      const std::u16string& selected_text);

  // Shows the selection affordance UI (widget or nudge).
  // Virtual for testing.
  virtual void ShowSelectionAffordance(const std::u16string& selected_text,
                                       BrowserWindowInterface* bwi);

  // content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void PrimaryPageChanged(content::Page& page) override;
  void OnWebContentsLostFocus(
      content::RenderWidgetHost* render_widget_host) override;

  // content::RenderWidgetHost::InputEventObserver:
  void OnInputEvent(
      const content::RenderWidgetHost& host,
      const blink::WebInputEvent& event,
      content::RenderWidgetHost::InputEventObserver::InputEventSource source)
      override;

 private:
  void ProcessPendingSelection();
  void ResetPendingSelection();

  void OnGlobalPanelShowHide();

  static void InvokeGlicFromSelectionAffordance(
      std::u16string selected_text,
      bool is_widget,
      base::WeakPtr<content::WebContents> web_contents,
      GlicNudgeActivity activity);


  bool ShouldShowSelectionWidget();
  void OnWidgetDismissed();
  void OnWidgetPinToggled(bool is_pinned);

  void CopyLinkToHighlight(content::WeakDocumentPtr weak_document_ptr);

  void WriteLinkToClipboard(content::WeakDocumentPtr weak_document_ptr,
                            const GURL& url);

  void OnLinkGenerated(
      const GURL& fallback_url,
      const std::string& selector,
      shared_highlighting::LinkGenerationError error,
      shared_highlighting::LinkGenerationReadyStatus ready_status);

  void RequestLinkGeneration(content::RenderFrameHost* rfh);

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
  // Preserves the widget's pinned state across subsequent selection updates.
  bool is_widget_pinned_ = false;
  // True during active user selection (mouse drag or key hold) to defer UI
  // updates until the input event completes.
  bool is_selecting_ = false;

  void OnAskGemini();
  void OnCopy();
  void OnCopyLink();
  void OnPinToggled(bool is_pinned);
  void OnDismiss();

  // Private bridge implementation of
  // GlicSelectionWidgetDelegate::ActionDelegate. This is required because
  // GlicSelectionObserver (in the //chrome/browser/glic) cannot directly
  // implement the UI-defined ActionDelegate interface to prevent circular
  // target dependencies in the build configuration.
  class WidgetActionDelegate;

  void OnWidgetClosed(views::Widget::ClosedReason reason);

  std::unique_ptr<views::Widget> selection_widget_;
  std::unique_ptr<GlicSelectionWidgetDelegate> widget_delegate_;
  std::unique_ptr<WidgetActionDelegate> action_delegate_;

  mojo::Remote<blink::mojom::TextFragmentReceiver> text_fragment_remote_;
  std::optional<GURL> generated_link_;

  base::WeakPtrFactory<GlicSelectionObserver> weak_ptr_factory_{this};

  friend class GlicSelectionObserverTest;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_SELECTION_OBSERVER_H_
