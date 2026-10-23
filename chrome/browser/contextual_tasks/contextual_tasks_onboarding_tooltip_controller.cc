// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_onboarding_tooltip_controller.h"

#include <utility>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_onboarding_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/contextual_tasks/public/features.h"
#include "components/feature_engagement/public/tracker.h"
#include "ui/base/interaction/element_events.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace contextual_tasks {

namespace {

gfx::NativeView GetBrowserParentWindow(BrowserWindowInterface* browser_window) {
  if (!browser_window) {
    return gfx::NativeView();
  }
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_window);
  if (!browser_view || !browser_view->GetWidget()) {
    return gfx::NativeView();
  }
  return browser_view->GetWidget()->GetNativeView();
}

}  // namespace

ContextualTasksOnboardingTooltipController::
    ContextualTasksOnboardingTooltipController(
        BrowserWindowInterface* browser_window,
        Profile* profile)
    : browser_window_(browser_window), profile_(profile) {}

ContextualTasksOnboardingTooltipController::
    ~ContextualTasksOnboardingTooltipController() {
  CloseBubble();
}

void ContextualTasksOnboardingTooltipController::SetEntryPoint(
    omnibox::ChromeAimEntryPoint entry_point) {
  if (entry_point_ != entry_point) {
    CloseBubbleWidget();
  }
  entry_point_ = entry_point;
  EnsureSubscribed();
  if (ui::ElementContext context = GetElementContext()) {
    if (ui::TrackedElement* element =
            ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kContextualTasksFaviconsElementId, context)) {
      MaybeShowBubble(element);
    }
  }
}

void ContextualTasksOnboardingTooltipController::CloseBubble() {
  entry_point_.reset();
  CloseBubbleWidget();
}

bool ContextualTasksOnboardingTooltipController::IsBubbleShowing() const {
  return bubble_widget_ != nullptr && !bubble_widget_->IsClosed();
}

void ContextualTasksOnboardingTooltipController::SetContextOverrideForTesting(
    ui::ElementContext context) {
  CHECK_IS_TEST();
  context_override_for_testing_ = context;
}

void ContextualTasksOnboardingTooltipController::OnWidgetDestroying(
    views::Widget* widget) {
  if (widget == bubble_widget_.get()) {
    active_variant_.reset();
    bubble_widget_observation_.Reset();
  }
}

void ContextualTasksOnboardingTooltipController::EnsureSubscribed() {
  if (is_subscribed_) {
    return;
  }
  ui::ElementContext context = GetElementContext();
  if (!context) {
    return;
  }
  is_subscribed_ = true;
  auto* tracker = ui::ElementTracker::GetElementTracker();
  // Safe because `element_subscriptions_` is owned by `this`.
  element_subscriptions_.push_back(tracker->AddElementShownCallback(
      kContextualTasksFaviconsElementId, context,
      base::BindRepeating(
          &ContextualTasksOnboardingTooltipController::OnFaviconsElementShown,
          base::Unretained(this))));
  element_subscriptions_.push_back(tracker->AddElementHiddenCallback(
      kContextualTasksFaviconsElementId, context,
      base::BindRepeating(
          &ContextualTasksOnboardingTooltipController::OnFaviconsElementHidden,
          base::Unretained(this))));
  element_subscriptions_.push_back(tracker->AddCustomEventCallback(
      ui::kElementBoundsChangedEvent, context,
      base::BindRepeating(&ContextualTasksOnboardingTooltipController::
                              OnFaviconsElementBoundsChanged,
                          base::Unretained(this))));
}

void ContextualTasksOnboardingTooltipController::CloseBubbleWidget() {
  active_variant_.reset();
  if (bubble_widget_) {
    bubble_widget_observation_.Reset();
    std::unique_ptr<views::Widget> widget = std::move(bubble_widget_);
    if (!widget->IsClosed()) {
      widget->CloseNow();
    }
  }
}

void ContextualTasksOnboardingTooltipController::OnFaviconsElementShown(
    ui::TrackedElement* element) {
  MaybeShowBubble(element);
}

void ContextualTasksOnboardingTooltipController::OnFaviconsElementHidden(
    ui::TrackedElement* element) {
  CloseBubbleWidget();
}

void ContextualTasksOnboardingTooltipController::OnFaviconsElementBoundsChanged(
    ui::TrackedElement* element) {
  if (!element || element->identifier() != kContextualTasksFaviconsElementId) {
    return;
  }
  if (IsBubbleShowing()) {
    if (views::WidgetDelegate* widget_delegate =
            bubble_widget_->widget_delegate()) {
      if (auto* delegate = widget_delegate->AsBubbleDialogDelegate()) {
        delegate->OnAnchorBoundsChanged();
      }
    }
  }
}

void ContextualTasksOnboardingTooltipController::MaybeShowBubble(
    ui::TrackedElement* anchor_element) {
  if (IsBubbleShowing() || !anchor_element || !entry_point_.has_value()) {
    return;
  }
  if (bubble_widget_) {
    CloseBubbleWidget();
  }
  if (ShouldShowVariant(
          ContextualTasksOnboardingBubbleView::Variant::kOnboarding)) {
    bubble_widget_ = ContextualTasksOnboardingBubbleView::CreateAndShow(
        views::BubbleAnchor(anchor_element),
        ContextualTasksOnboardingBubbleView::Variant::kOnboarding,
        base::BindOnce(
            &ContextualTasksOnboardingTooltipController::OnAcceptPressed,
            weak_ptr_factory_.GetWeakPtr(),
            ContextualTasksOnboardingBubbleView::Variant::kOnboarding),
        base::BindRepeating(
            &ContextualTasksOnboardingTooltipController::OnLearnMorePressed,
            weak_ptr_factory_.GetWeakPtr(),
            ContextualTasksOnboardingBubbleView::Variant::kOnboarding),
        GetBrowserParentWindow(browser_window_));
    if (bubble_widget_) {
      active_variant_ =
          ContextualTasksOnboardingBubbleView::Variant::kOnboarding;
      bubble_widget_observation_.Observe(bubble_widget_.get());
    }
  }
}

bool ContextualTasksOnboardingTooltipController::ShouldShowVariant(
    ContextualTasksOnboardingBubbleView::Variant variant) const {
  if (!IsContextualTasksRearchitectureEnabled() &&
      !IsContextualTasksSidePanelRearchitectureEnabled()) {
    return false;
  }
  return profile_ &&
         base::FeatureList::IsEnabled(kContextualTasksShowOnboardingTooltip);
}

void ContextualTasksOnboardingTooltipController::OnAcceptPressed(
    ContextualTasksOnboardingBubbleView::Variant variant) {
  active_variant_.reset();
  if (profile_ &&
      variant == ContextualTasksOnboardingBubbleView::Variant::kOnboarding) {
    if (auto* tracker =
            feature_engagement::TrackerFactory::GetForBrowserContext(
                profile_)) {
      tracker->NotifyEvent("contextual_tasks_onboarding_dismissed");
    }
  }
}

void ContextualTasksOnboardingTooltipController::OnLearnMorePressed(
    ContextualTasksOnboardingBubbleView::Variant variant) {
  if (!browser_window_) {
    return;
  }
  GURL help_url(variant ==
                        ContextualTasksOnboardingBubbleView::Variant::kAskGoogle
                    ? GetContextualTasksTabHelpUrl()
                    : GetContextualTasksOnboardingTooltipHelpUrl());
  if (help_url.is_valid()) {
    browser_window_->OpenGURL(help_url,
                              WindowOpenDisposition::kNewForegroundTab);
  }
}

ui::ElementContext
ContextualTasksOnboardingTooltipController::GetElementContext() const {
  if (context_override_for_testing_) {
    CHECK_IS_TEST();
    return context_override_for_testing_;
  }
  if (!browser_window_) {
    return ui::ElementContext();
  }
  auto* browser_elements = BrowserElements::From(browser_window_);
  return browser_elements ? browser_elements->GetContext()
                          : ui::ElementContext();
}

}  // namespace contextual_tasks
