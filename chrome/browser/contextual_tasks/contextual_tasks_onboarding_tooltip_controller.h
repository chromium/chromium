// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_ONBOARDING_TOOLTIP_CONTROLLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_ONBOARDING_TOOLTIP_CONTROLLER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_onboarding_bubble_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/widget/widget_observer.h"

class BrowserWindowInterface;
class Profile;

namespace blink {
class WebInputEvent;
}  // namespace blink

namespace content {
class WebContents;
}  // namespace content

namespace contextual_search {
class ContextualSearchSessionHandle;
struct TabContextState;
}  // namespace contextual_search

namespace contextual_tasks {

// Manages the Contextual Tasks privacy notice and Ask Google tooltip bubbles.
class ContextualTasksOnboardingTooltipController
    : public views::WidgetObserver {
 public:
  // `browser_window` and `profile` may be null in tests and must outlive
  // `this` when non-null.
  ContextualTasksOnboardingTooltipController(
      BrowserWindowInterface* browser_window,
      Profile* profile);
  ContextualTasksOnboardingTooltipController(
      const ContextualTasksOnboardingTooltipController&) = delete;
  ContextualTasksOnboardingTooltipController& operator=(
      const ContextualTasksOnboardingTooltipController&) = delete;
  ~ContextualTasksOnboardingTooltipController() override;

  void SetEntryPoint(omnibox::ChromeAimEntryPoint entry_point);
  void CloseBubble();
  bool IsBubbleShowing() const;

  void SetContextOverrideForTesting(ui::ElementContext context);
  void SetHasAutomaticActiveTabForTesting(
      std::optional<bool> has_automatic_active_tab);
  std::optional<ContextualTasksOnboardingBubbleView::Variant>
  active_variant_for_testing() const {
    return active_variant_;
  }
  views::Widget* bubble_widget_for_testing() { return bubble_widget_.get(); }
  bool is_ask_google_entry_point_eligible_for_testing() const {
    return IsAskGoogleEntryPointEligible();
  }
  content::WebContents* observed_active_web_contents_for_testing() {
    return active_web_contents_observer_.web_contents();
  }
  content::WebContents* observed_toolbar_web_contents_for_testing() {
    return toolbar_web_contents_observer_.web_contents();
  }

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  class PanelWebContentsObserver : public content::WebContentsObserver {
   public:
    explicit PanelWebContentsObserver(
        ContextualTasksOnboardingTooltipController& controller);
    ~PanelWebContentsObserver() override;

    void ObserveWebContents(content::WebContents* web_contents);

    // content::WebContentsObserver:
    void DidGetUserInteraction(const blink::WebInputEvent& event) override;

   private:
    const raw_ref<ContextualTasksOnboardingTooltipController> controller_;
  };

  void EnsureSubscribed();
  void RefreshWebContentsObservations();
  void RefreshTabContextSubscription();
  void OnTabContextUpdated(const contextual_search::TabContextState& state);
  void CloseBubbleWidget();
  void OnFaviconsElementShown(ui::TrackedElement* element);
  void OnFaviconsElementHidden(ui::TrackedElement* element);
  void OnFaviconsElementBoundsChanged(ui::TrackedElement* element);
  void MaybeShowBubble(ui::TrackedElement* anchor_element);
  bool IsAskGoogleEntryPointEligible() const;
  bool HasAutomaticActiveTab() const;
  bool ShouldShowVariant(
      ContextualTasksOnboardingBubbleView::Variant variant) const;
  void OnAcceptPressed(ContextualTasksOnboardingBubbleView::Variant variant);
  void OnLearnMorePressed(ContextualTasksOnboardingBubbleView::Variant variant);
  void OnImpressionTimerFired(
      ContextualTasksOnboardingBubbleView::Variant variant);
  ui::ElementContext GetElementContext() const;
  contextual_search::ContextualSearchSessionHandle* GetSessionHandle();
  const contextual_search::ContextualSearchSessionHandle* GetSessionHandle()
      const;

  const raw_ptr<BrowserWindowInterface> browser_window_;
  const raw_ptr<Profile> profile_;
  std::optional<omnibox::ChromeAimEntryPoint> entry_point_;
  std::optional<ContextualTasksOnboardingBubbleView::Variant> active_variant_;
  std::unique_ptr<views::Widget> bubble_widget_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      bubble_widget_observation_{this};
  base::OneShotTimer impression_delay_timer_;
  std::vector<ui::ElementTracker::Subscription> element_subscriptions_;
  bool is_subscribed_ = false;
  base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
      subscribed_session_handle_;
  base::CallbackListSubscription tab_context_subscription_;
  int onboarding_session_impression_count_ = 0;
  int ask_google_session_impression_count_ = 0;
  ui::ElementContext context_override_for_testing_;
  std::optional<bool> has_automatic_active_tab_for_testing_;
  PanelWebContentsObserver active_web_contents_observer_{*this};
  PanelWebContentsObserver toolbar_web_contents_observer_{*this};

  base::WeakPtrFactory<ContextualTasksOnboardingTooltipController>
      weak_ptr_factory_{this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_ONBOARDING_TOOLTIP_CONTROLLER_H_
