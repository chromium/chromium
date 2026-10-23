// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_onboarding_tooltip_controller.h"

#include <utility>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_onboarding_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/prefs.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
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
  RefreshTabContextSubscription();

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

void ContextualTasksOnboardingTooltipController::
    SetHasAutomaticActiveTabForTesting(
        std::optional<bool> has_automatic_active_tab) {
  CHECK_IS_TEST();
  has_automatic_active_tab_for_testing_ = has_automatic_active_tab;
  OnTabContextUpdated(contextual_search::TabContextState());
}

void ContextualTasksOnboardingTooltipController::OnWidgetDestroying(
    views::Widget* widget) {
  if (widget == bubble_widget_.get()) {
    impression_delay_timer_.Stop();
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

void ContextualTasksOnboardingTooltipController::
    RefreshTabContextSubscription() {
  contextual_search::ContextualSearchSessionHandle* session_handle =
      GetSessionHandle();
  if (subscribed_session_handle_.get() == session_handle) {
    return;
  }
  if (!session_handle) {
    subscribed_session_handle_ = nullptr;
    tab_context_subscription_ = {};
    return;
  }
  subscribed_session_handle_ = session_handle->AsWeakPtr();
  // Safe because `tab_context_subscription_` is owned by `this`.
  tab_context_subscription_ =
      session_handle->SubscribeTabContext(base::BindRepeating(
          &ContextualTasksOnboardingTooltipController::OnTabContextUpdated,
          base::Unretained(this)));
}

void ContextualTasksOnboardingTooltipController::OnTabContextUpdated(
    const contextual_search::TabContextState& state) {
  if (active_variant_ ==
          ContextualTasksOnboardingBubbleView::Variant::kOnboarding &&
      !HasAutomaticActiveTab()) {
    CloseBubbleWidget();
    return;
  }
  if (!IsBubbleShowing() && HasAutomaticActiveTab()) {
    if (ui::ElementContext context = GetElementContext()) {
      if (ui::TrackedElement* element =
              ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                  kContextualTasksFaviconsElementId, context)) {
        MaybeShowBubble(element);
      }
    }
  }
}

void ContextualTasksOnboardingTooltipController::CloseBubbleWidget() {
  impression_delay_timer_.Stop();
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
  RefreshTabContextSubscription();
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

// LINT.IfChange(ContextualTasksTooltipLogic)
void ContextualTasksOnboardingTooltipController::MaybeShowBubble(
    ui::TrackedElement* anchor_element) {
  if (IsBubbleShowing() || !anchor_element || !entry_point_.has_value()) {
    return;
  }
  if (bubble_widget_) {
    CloseBubbleWidget();
  }
  if (IsAskGoogleEntryPointEligible() &&
      base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxAskGAboutThisPage)) {
    if (ShouldShowVariant(
            ContextualTasksOnboardingBubbleView::Variant::kAskGoogle)) {
      bubble_widget_ = ContextualTasksOnboardingBubbleView::CreateAndShow(
          views::BubbleAnchor(anchor_element),
          ContextualTasksOnboardingBubbleView::Variant::kAskGoogle,
          base::BindOnce(
              &ContextualTasksOnboardingTooltipController::OnAcceptPressed,
              weak_ptr_factory_.GetWeakPtr(),
              ContextualTasksOnboardingBubbleView::Variant::kAskGoogle),
          base::BindRepeating(
              &ContextualTasksOnboardingTooltipController::OnLearnMorePressed,
              weak_ptr_factory_.GetWeakPtr(),
              ContextualTasksOnboardingBubbleView::Variant::kAskGoogle),
          GetBrowserParentWindow(browser_window_));

      if (bubble_widget_) {
        active_variant_ =
            ContextualTasksOnboardingBubbleView::Variant::kAskGoogle;
        ask_google_session_impression_count_++;
        bubble_widget_observation_.Observe(bubble_widget_.get());
      }
    }
    return;
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
      impression_delay_timer_.Start(
          FROM_HERE,
          base::Milliseconds(
              GetContextualTasksOnboardingTooltipImpressionDelay()),
          base::BindOnce(
              &ContextualTasksOnboardingTooltipController::
                  OnImpressionTimerFired,
              weak_ptr_factory_.GetWeakPtr(),
              ContextualTasksOnboardingBubbleView::Variant::kOnboarding));
    }
  }
}

bool ContextualTasksOnboardingTooltipController::IsAskGoogleEntryPointEligible()
    const {
  return entry_point_ == omnibox::ChromeAimEntryPoint::
                             DESKTOP_CHROME_COBROWSE_OMNIBOX_TAB_SEARCH ||
         entry_point_ == omnibox::ChromeAimEntryPoint::
                             DESKTOP_CHROME_COBROWSE_OMNIBOX_ACTION ||
         entry_point_ ==
             omnibox::ChromeAimEntryPoint::
                 DESKTOP_CHROME_COBROWSE_OMNIBOX_CONTEXTUAL_SUGGESTION ||
         entry_point_ == omnibox::ChromeAimEntryPoint::
                             DESKTOP_CHROME_COBROWSE_OMNIBOX_POPUP_BUTTON;
}

bool ContextualTasksOnboardingTooltipController::HasAutomaticActiveTab() const {
  if (has_automatic_active_tab_for_testing_.has_value()) {
    CHECK_IS_TEST();
    return *has_automatic_active_tab_for_testing_;
  }
  if (const contextual_search::ContextualSearchSessionHandle* session_handle =
          GetSessionHandle()) {
    return !session_handle->GetPendingDelayedTabIds().empty();
  }
  return false;
}

bool ContextualTasksOnboardingTooltipController::ShouldShowVariant(
    ContextualTasksOnboardingBubbleView::Variant variant) const {
  if (!IsContextualTasksRearchitectureEnabled() &&
      !IsContextualTasksSidePanelRearchitectureEnabled()) {
    return false;
  }
  if (!profile_ || !profile_->GetPrefs()) {
    return false;
  }
  PrefService* prefs = profile_->GetPrefs();
  bool bypass_cap =
      base::FeatureList::IsEnabled(kContextualTasksBypassDismissedCap);

  if (variant == ContextualTasksOnboardingBubbleView::Variant::kAskGoogle) {
    if (!base::FeatureList::IsEnabled(
            omnibox::kWebUIOmniboxAskGAboutThisPage)) {
      return false;
    }
    if (!bypass_cap &&
        prefs->GetInteger(kContextualTasksAskGTooltipDismissedCount) >=
            GetContextualTasksAskGTooltipDismissedCap()) {
      return false;
    }
    return ask_google_session_impression_count_ <
           GetContextualTasksAskGTooltipSessionImpressionCap();
  }

  if (!base::FeatureList::IsEnabled(kContextualTasksShowOnboardingTooltip) ||
      !HasAutomaticActiveTab()) {
    return false;
  }
  if (!bypass_cap &&
      prefs->GetInteger(kContextualTasksOnboardingTooltipDismissedCount) >=
          GetContextualTasksOnboardingTooltipDismissedCap()) {
    return false;
  }
  return onboarding_session_impression_count_ <
         GetContextualTasksShowOnboardingTooltipSessionImpressionCap();
}
// LINT.ThenChange(//chrome/browser/resources/contextual_tasks/app.ts:ContextualTasksTooltipLogic)

void ContextualTasksOnboardingTooltipController::OnAcceptPressed(
    ContextualTasksOnboardingBubbleView::Variant variant) {
  impression_delay_timer_.Stop();
  active_variant_.reset();

  if (profile_) {
    if (PrefService* prefs = profile_->GetPrefs()) {
      if (variant == ContextualTasksOnboardingBubbleView::Variant::kAskGoogle) {
        prefs->SetInteger(
            kContextualTasksAskGTooltipDismissedCount,
            prefs->GetInteger(kContextualTasksAskGTooltipDismissedCount) + 1);
      } else {
        prefs->SetInteger(
            kContextualTasksOnboardingTooltipDismissedCount,
            prefs->GetInteger(kContextualTasksOnboardingTooltipDismissedCount) +
                1);
        prefs->SetInteger(kContextualTasksSessionCountPostOnboarding, 0);
      }
    }

    if (variant == ContextualTasksOnboardingBubbleView::Variant::kOnboarding) {
      if (auto* tracker =
              feature_engagement::TrackerFactory::GetForBrowserContext(
                  profile_)) {
        tracker->NotifyEvent("contextual_tasks_onboarding_dismissed");
      }
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

void ContextualTasksOnboardingTooltipController::OnImpressionTimerFired(
    ContextualTasksOnboardingBubbleView::Variant variant) {
  if (variant == ContextualTasksOnboardingBubbleView::Variant::kOnboarding) {
    onboarding_session_impression_count_++;
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

contextual_search::ContextualSearchSessionHandle*
ContextualTasksOnboardingTooltipController::GetSessionHandle() {
  if (!browser_window_) {
    return nullptr;
  }
  auto* panel_controller =
      ContextualTasksPanelController::From(browser_window_);
  return panel_controller
             ? panel_controller->GetContextualSearchSessionHandleForPanel()
             : nullptr;
}

const contextual_search::ContextualSearchSessionHandle*
ContextualTasksOnboardingTooltipController::GetSessionHandle() const {
  if (!browser_window_) {
    return nullptr;
  }
  auto* panel_controller =
      ContextualTasksPanelController::From(browser_window_);
  return panel_controller
             ? panel_controller->GetContextualSearchSessionHandleForPanel()
             : nullptr;
}

}  // namespace contextual_tasks
