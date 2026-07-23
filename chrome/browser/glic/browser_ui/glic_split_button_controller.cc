// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_split_button_controller.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller_impl.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_delegate.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/common/chrome_features.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "chrome/browser/glic/browser_ui/glic_actor_nudge_controller.h"
#include "chrome/browser/glic/browser_ui/glic_button_controller.h"
#endif

namespace glic {

DEFINE_USER_DATA(GlicSplitButtonController);

// static
GlicSplitButtonController* GlicSplitButtonController::From(
    BrowserWindowInterface* browser) {
  return browser
             ? GlicSplitButtonController::Get(browser->GetUnownedUserDataHost())
             : nullptr;
}

GlicSplitButtonController::GlicSplitButtonController(
    BrowserWindowInterface* browser,
    GlicKeyedService* glic_service)
    : browser_(browser),
      scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this) {
  CHECK(glic_service);
  glic_nudge_controller_ =
      std::make_unique<GlicNudgeControllerImpl>(browser, this);

  // TODO(crbug.com/518584352): Port these to Android.
#if !BUILDFLAG(IS_ANDROID)
  glic_button_controller_ = std::make_unique<GlicButtonController>(
      browser->GetProfile(), *browser, this, glic_service);

  if (base::FeatureList::IsEnabled(features::kGlicActor) &&
      base::FeatureList::IsEnabled(features::kGlicActorUi) &&
      features::kGlicActorUiTaskIcon.Get() &&
      browser->GetProfile()->IsRegularProfile()) {
    actor_task_list_bubble_controller_ =
        std::make_unique<ActorTaskListBubbleController>(browser);
    glic_actor_nudge_controller_ =
        std::make_unique<GlicActorNudgeController>(browser, this);
  }
#endif
}

GlicSplitButtonController::~GlicSplitButtonController() = default;

void GlicSplitButtonController::SetHorizontalTabsDelegate(
    GlicSplitButtonDelegate* delegate) {
  horizontal_tabs_delegate_ = delegate;
#if !BUILDFLAG(IS_ANDROID)
  glic_button_controller_->UpdateButton();
#endif
}

void GlicSplitButtonController::SetVerticalTabsDelegate(
    GlicSplitButtonDelegate* delegate) {
  vertical_tabs_delegate_ = delegate;
#if !BUILDFLAG(IS_ANDROID)
  glic_button_controller_->UpdateButton();
#endif
}

void GlicSplitButtonController::CallOnBoth(
    base::RepeatingCallback<void(GlicSplitButtonDelegate&)> fn) {
  if (horizontal_tabs_delegate_ && horizontal_tabs_delegate_->IsGlicAdded()) {
    fn.Run(*horizontal_tabs_delegate_);
  }
  if (vertical_tabs_delegate_ && vertical_tabs_delegate_->IsGlicAdded()) {
    fn.Run(*vertical_tabs_delegate_);
  }
}

GlicSplitButtonDelegate* GlicSplitButtonController::GetActiveDelegate() {
#if BUILDFLAG(IS_ANDROID)
  return horizontal_tabs_delegate_;
#else
  auto* vertical_tab_strip_state_controller =
      tabs::VerticalTabStripStateController::From(browser_);

  GlicSplitButtonDelegate* delegate =
      vertical_tab_strip_state_controller &&
              vertical_tab_strip_state_controller->ShouldDisplayVerticalTabs()
          ? vertical_tabs_delegate_
          : horizontal_tabs_delegate_;
  if (delegate && delegate->IsGlicAdded()) {
    return delegate;
  }
  return nullptr;
#endif
}

base::WeakPtr<GlicSplitButtonController>
GlicSplitButtonController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace glic
