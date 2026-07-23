// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_split_button_controller.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_delegate.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
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
    : scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this) {
  CHECK(glic_service);
  glic_nudge_controller_ = GlicNudgeController::CreateFor(browser);

  // TODO(crbug.com/518584352): Port these to Android.
#if !BUILDFLAG(IS_ANDROID)
  glic_button_controller_ = std::make_unique<GlicButtonController>(
      browser->GetProfile(), *browser, glic_service);

  if (base::FeatureList::IsEnabled(features::kGlicActor) &&
      base::FeatureList::IsEnabled(features::kGlicActorUi) &&
      features::kGlicActorUiTaskIcon.Get() &&
      browser->GetProfile()->IsRegularProfile()) {
    actor_task_list_bubble_controller_ =
        std::make_unique<ActorTaskListBubbleController>(browser);
    glic_actor_nudge_controller_ =
        std::make_unique<GlicActorNudgeController>(browser);
  }
#endif
}

GlicSplitButtonController::~GlicSplitButtonController() = default;

void GlicSplitButtonController::SetHorizontalTabsDelegate(
    GlicSplitButtonDelegate* delegate) {
  glic_nudge_controller_->SetHorizontalTabsDelegate(delegate);
#if !BUILDFLAG(IS_ANDROID)
  glic_button_controller_->SetHorizontalTabsDelegate(delegate);
  if (glic_actor_nudge_controller_) {
    glic_actor_nudge_controller_->SetHorizontalTabsDelegate(delegate);
  }
#endif
}

void GlicSplitButtonController::SetVerticalTabsDelegate(
    GlicSplitButtonDelegate* delegate) {
  glic_nudge_controller_->SetVerticalTabsDelegate(delegate);
#if !BUILDFLAG(IS_ANDROID)
  glic_button_controller_->SetVerticalTabsDelegate(delegate);
  if (glic_actor_nudge_controller_) {
    glic_actor_nudge_controller_->SetVerticalTabsDelegate(delegate);
  }
#endif
}

base::WeakPtr<GlicSplitButtonController>
GlicSplitButtonController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace glic
