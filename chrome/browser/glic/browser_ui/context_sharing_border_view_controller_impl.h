// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_CONTEXT_SHARING_BORDER_VIEW_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_CONTEXT_SHARING_BORDER_VIEW_CONTROLLER_IMPL_H_

#include <list>
#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/glic/browser_ui/context_sharing_border_view_controller.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/views/view_observer.h"

class Browser;
class ContentsWebView;

namespace content {
class WebContents;
}

namespace glic {
class ContextSharingBorderView;

class ContextSharingBorderViewControllerImpl
    : public ContextSharingBorderViewController,
      public views::ViewObserver {
 public:
  ContextSharingBorderViewControllerImpl();
  ContextSharingBorderViewControllerImpl(
      const ContextSharingBorderViewControllerImpl&) = delete;
  ContextSharingBorderViewControllerImpl& operator=(
      const ContextSharingBorderViewControllerImpl&) = delete;
  ~ContextSharingBorderViewControllerImpl() override;

  // ContextSharingBorderViewController overrides:
  void Initialize(ContextSharingBorderView* border_view,
                  ContentsWebView* contents_web_view,
                  Browser* browser) override;
  ContentsWebView* contents_web_view() override;
  bool IsSidePanelOpen() const override;

 private:
  // Called when the focused tab changes with the focused tab data object.
  void OnFocusedTabChanged(const FocusedTabData& focused_tab_data);

  // Called when the actor component changes the border glow status.
  void OnActorBorderGlowUpdated(tabs::TabInterface* tab, bool enabled);

  // Called when the client changes the context access indicator status.
  void OnIndicatorStatusChanged(bool enabled);

  // ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override;

  // Updates the BorderView UI effect given the current state of the focused tab
  // and context access indicator flag.
  enum class UpdateBorderReason {
    kContextAccessIndicatorOn = 0,
    kContextAccessIndicatorOff,

    // Tab focus changes in the same contents view.
    kFocusedTabChanged_NoFocusChange,

    // Focus changes across different contents view.
    kFocusedTabChanged_GainFocus,
    kFocusedTabChanged_LostFocus,
  };

  // This function is a gateway for all non actor border updates. It respects
  // the actor_border_glow_enabled_ flag, which can suppress or override regular
  // updates. It also keeps track of the last reason for an update.
  void MaybeRunBorderViewUpdate(UpdateBorderReason reason);

  void UpdateBorderView(UpdateBorderReason reason);

  bool IsGlicWindowShowing() const;

  bool IsTabInCurrentView(const content::WebContents* tab) const;

  bool ShouldShowBorderAnimation();

  std::string UpdateReasonToString(UpdateBorderReason reason);

  void AddReasonForDebugging(UpdateBorderReason reason);

  std::string UpdateReasonsToString() const;

  // Back pointer to the owner. Set after Initialize(). Guaranteed to outlive
  // `this`.
  raw_ptr<ContextSharingBorderView> border_view_;

  // Pointer to the associated contents web view and associated view
  // observation for view deletion. Set after Initialize().
  raw_ptr<ContentsWebView> contents_web_view_;

  // The Glic keyed service. Set after Initialize().
  raw_ptr<GlicKeyedService> glic_service_;
  base::ScopedObservation<views::View, views::ViewObserver>
      contents_web_view_observation_{this};

  // Tracked states and their subscriptions.
  base::WeakPtr<content::WebContents> glic_focused_contents_in_current_view_;
  base::CallbackListSubscription focus_change_subscription_;
  bool context_access_indicator_enabled_ = false;
  base::CallbackListSubscription indicator_change_subscription_;

  // When true, the actor framework has requested the border to glow. This
  // overrides other signals.
  bool actor_border_glow_enabled_ = false;

  // Subscription to the actor border controller for glow updates.
  base::CallbackListSubscription actor_border_view_controller_subscription_;

  static constexpr size_t kNumReasonsToKeep = 10u;
  std::list<std::string> border_update_reasons_;

  // Stores the last mutating reason for a border update, so the state can be
  // restored when the actor glow is disabled.
  std::optional<UpdateBorderReason> last_mutating_update_reason_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_CONTEXT_SHARING_BORDER_VIEW_CONTROLLER_IMPL_H_
