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
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/host/host.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_observer.h"
#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#include "components/skills/public/skill.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/link_to_text/link_to_text.mojom.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace content {
class Page;
class RenderFrameHost;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

class BrowserWindowInterface;
enum class ToastId;

namespace optimization_guide {
class PageContextEligibilityObserver;
class PageContextEligibility;
}  // namespace optimization_guide

namespace glic {

class ExplainSelectionTrigger;
class GlicSelectionWidgetDelegate;
class GlicKeyedService;

using GlicSkillOption = skills::Skill;

class GlicSelectionObserver
    : public content::WebContentsObserver,
      public content::RenderWidgetHost::InputEventObserver,
      public content_settings::Observer {
 public:
  DECLARE_USER_DATA(GlicSelectionObserver);

  enum class DismissReason {
    kActionTaken,  // User clicked Ask Gemini, Copy, Copy Link, or Open in Side
                   // Panel.
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

  void OnTextSelectionChanged(content::RenderFrameHost* render_frame_host,
                              std::u16string_view selected_text) override;

  // Notifies the observer that text selection context was sent to the Glic
  // panel from the context menu entry point.
  void UpdateSelectionStateFromContextMenu(const std::u16string& selected_text);

  bool has_sent_selection_context() const {
    return has_sent_selection_context_;
  }

 protected:
  // Updates the Glic UI (nudge or panel) with the selected text.
  // Virtual for testing.
  virtual void UpdateSelectionState(const std::u16string& text,
                                    bool is_pending_selection,
                                    SelectionSource source);

  // Dismisses the selection UI (widget and/or nudge).
  // Virtual for testing.
  virtual void DismissUI(DismissReason reason);

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

  // Returns true if the selection widget should be shown for the current page.
  bool ShouldShowSelectionWidget();

  // Triggers Glic region capture when a mouse shake is detected.
  // Virtual for testing.
  virtual void TriggerRegionCapture();

  // Shows the selection overlay.
  // Virtual for testing.
  virtual void ShowSelectionOverlay();

  // Returns true if mouse shake trigger is enabled by feature flag and pref.
  // Virtual for testing.
  virtual bool IsShakeTriggerEnabled() const;

  // Returns true if the Glic side panel is open.
  // Virtual for testing.
  virtual bool IsSidePanelOpen() const;

  // Called when the page context eligibility changes.
  // Virtual for testing.
  virtual void OnPageContextEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus status);

  // content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void PrimaryPageChanged(content::Page& page) override;
  void PrimaryMainFrameWasResized(bool width_changed) override;
  void OnWebContentsLostFocus(
      content::RenderWidgetHost* render_widget_host) override;

  // content::RenderWidgetHost::InputEventObserver:
  void OnInputEvent(
      const content::RenderWidgetHost& host,
      const blink::WebInputEvent& event,
      content::RenderWidgetHost::InputEventObserver::InputEventSource source)
      override;

  // content_settings::Observer:
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

 private:
  void UpdatePageBlockedState();
  void ProcessPendingSelection();
  void ResetPendingSelection();
  void ProcessInputEvent(std::unique_ptr<blink::WebInputEvent> event);

  void OnGlobalPanelShowHide();

  static void InvokeGlicFromSelectionAffordance(
      std::u16string selected_text,
      bool is_widget,
      base::WeakPtr<content::WebContents> web_contents,
      std::u16string prompt_override = u"",
      const GlicSkillOption& skill = {},
      const std::string& skill_prompt = "");

  void OnAskGemini();
  void OnAskGeminiWithSkill(const GlicSkillOption& skill);
  std::vector<GlicSkillOption> GetContextualSkills();
  std::vector<GlicSkillOption> GetUserSkills();
  void OnAskGeminiForQuery(const std::u16string& query);
  void OnAskGeminiMoreAboutThis(const std::u16string& selected_text,
                                const std::string& explanation_text);
  void OnInlineExplanationUpdate(const std::string& markdown_output,
                                 bool is_complete,
                                 const std::string& error_message);
  void OnCopy();
  void OnCopyLink();
  void OnHide();
  void OnSettings();
  void ShowHiddenToast(ToastId toast_id);

  void CopyLinkToHighlight(content::WeakDocumentPtr weak_document_ptr);

  void WriteLinkToClipboard(content::WeakDocumentPtr weak_document_ptr,
                            const GURL& url);

  void OnLinkGenerated(
      const GURL& fallback_url,
      const std::string& selector,
      shared_highlighting::LinkGenerationError error,
      shared_highlighting::LinkGenerationReadyStatus ready_status);

  void RequestLinkGeneration(content::RenderFrameHost* rfh);
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
  // True when an inline explanation is currently being fetched or displayed.
  bool is_explaining_ = false;
  // True if a dismissal metric has already been recorded for the shown widget.
  bool dismissal_recorded_ = false;

  void ProcessMouseMoveForShake(const blink::WebMouseEvent& mouse_event);
  void ResetShakeDetector();

  std::optional<gfx::PointF> last_shake_point_;
  std::optional<gfx::Vector2dF> last_shake_dir_;
  int direction_change_count_ = 0;
  base::TimeTicks last_direction_change_time_;

  // Private bridge implementation of
  // GlicSelectionWidgetDelegate::ActionDelegate. This is required because
  // GlicSelectionObserver (in the //chrome/browser/glic) cannot directly
  // implement the UI-defined ActionDelegate interface to prevent circular
  // target dependencies in the build configuration.
  class WidgetActionDelegate;

  void OnWidgetClose();
  void OnOpenInSidePanel();

  std::unique_ptr<GlicSelectionWidgetDelegate> widget_delegate_;
  std::unique_ptr<WidgetActionDelegate> action_delegate_;
  mojo::Remote<blink::mojom::TextFragmentReceiver> text_fragment_remote_;
  std::optional<GURL> generated_link_;
  std::unique_ptr<ExplainSelectionTrigger> explain_selection_trigger_;

  friend class GlicSelectionObserverTest;
  FRIEND_TEST_ALL_PREFIXES(GlicSelectionObserverTest,
                           SelectionWordCountMetrics);

 protected:
  // True if the user temporarily blocked the selection widget for the current
  // page load.
  bool is_hidden_on_current_page_ = false;
  // True if the site is blocked from showing the inline cue by user settings or
  // default blocklist.
  bool is_site_blocked_on_current_page_ = false;

  bool IsPageContextEligible() const;

  ::optimization_guide::PageContextEligibilityObserver* page_context_tracker() {
    return page_context_tracker_.get();
  }

 private:
  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      content_settings_observation_{this};
  base::CallbackListSubscription page_context_eligibility_subscription_;
  std::unique_ptr<::optimization_guide::PageContextEligibilityObserver>
      page_context_tracker_;
  std::unique_ptr<ui::ScopedUnownedUserData<GlicSelectionObserver>>
      scoped_unowned_user_data_;
  base::WeakPtrFactory<GlicSelectionObserver> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_SELECTION_OBSERVER_H_
