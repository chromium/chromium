// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_SELECTION_OBSERVER_H_
#define CHROME_BROWSER_GLIC_GLIC_SELECTION_OBSERVER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_observer.h"

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
  virtual void UpdateSelectionState(const std::u16string& text);

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

  static void InvokeGlicFromSelectionAffordance(
      std::u16string selected_text,
      bool is_widget,
      base::WeakPtr<content::WebContents> web_contents,
      GlicNudgeActivity activity);

  void ShowSelectionAffordance(const std::u16string& selected_text,
                               BrowserWindowInterface* bwi);

  raw_ptr<GlicKeyedService> glic_keyed_service_;

  // Timer to process the selection after a timeout.
  base::OneShotTimer selection_debounce_timer_;

  // The text of the last selection that was ignored due to rate limiting.
  std::optional<std::u16string> pending_selection_text_;

  content::GlobalRenderFrameHostId last_selection_frame_id_;

  base::flat_map<content::GlobalRenderFrameHostId,
                 raw_ptr<content::RenderWidgetHost>>
      rwh_by_frame_;

  bool is_key_selection_ = false;
  int bounds_retry_count_ = 0;

  bool has_sent_selection_context_ = false;

  base::WeakPtr<views::Widget> selection_widget_;

  friend class GlicSelectionObserverTest;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_SELECTION_OBSERVER_H_
