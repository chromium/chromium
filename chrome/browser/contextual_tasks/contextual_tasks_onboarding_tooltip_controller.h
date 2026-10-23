// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_ONBOARDING_TOOLTIP_CONTROLLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_ONBOARDING_TOOLTIP_CONTROLLER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_onboarding_bubble_view.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/widget/widget_observer.h"

class BrowserWindowInterface;
class Profile;

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
  std::optional<ContextualTasksOnboardingBubbleView::Variant>
  active_variant_for_testing() const {
    return active_variant_;
  }
  views::Widget* bubble_widget_for_testing() { return bubble_widget_.get(); }

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  void EnsureSubscribed();
  void CloseBubbleWidget();
  void OnFaviconsElementShown(ui::TrackedElement* element);
  void OnFaviconsElementHidden(ui::TrackedElement* element);
  void OnFaviconsElementBoundsChanged(ui::TrackedElement* element);
  void MaybeShowBubble(ui::TrackedElement* anchor_element);
  bool ShouldShowVariant(
      ContextualTasksOnboardingBubbleView::Variant variant) const;
  void OnAcceptPressed(ContextualTasksOnboardingBubbleView::Variant variant);
  void OnLearnMorePressed(ContextualTasksOnboardingBubbleView::Variant variant);
  ui::ElementContext GetElementContext() const;

  const raw_ptr<BrowserWindowInterface> browser_window_;
  const raw_ptr<Profile> profile_;
  std::optional<omnibox::ChromeAimEntryPoint> entry_point_;
  std::optional<ContextualTasksOnboardingBubbleView::Variant> active_variant_;
  std::unique_ptr<views::Widget> bubble_widget_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      bubble_widget_observation_{this};
  std::vector<ui::ElementTracker::Subscription> element_subscriptions_;
  bool is_subscribed_ = false;
  ui::ElementContext context_override_for_testing_;

  base::WeakPtrFactory<ContextualTasksOnboardingTooltipController>
      weak_ptr_factory_{this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_ONBOARDING_TOOLTIP_CONTROLLER_H_
