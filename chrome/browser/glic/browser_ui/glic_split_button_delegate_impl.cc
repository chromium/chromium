// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_split_button_delegate_impl.h"

#include <utility>

#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

namespace glic {

GlicSplitButtonDelegateImpl::GlicSplitButtonDelegateImpl(
    BrowserWindowInterface* browser,
    GlicKeyedService* glic_service)
    : browser_(browser), glic_service_(glic_service) {
  CHECK(browser_);
  CHECK(glic_service_);
}

GlicSplitButtonDelegateImpl::~GlicSplitButtonDelegateImpl() = default;

base::CallbackListSubscription
GlicSplitButtonDelegateImpl::RegisterEnabledChangedCallback(
    base::RepeatingClosure callback) {
  return glic_service_->enabling().RegisterAllowedChanged(std::move(callback));
}

bool GlicSplitButtonDelegateImpl::IsEnabled() const {
  Profile* profile = browser_->GetProfile();
  return profile ? GlicEnabling::ShouldShowGlicButton(profile) : false;
}

bool GlicSplitButtonDelegateImpl::IsPanelShowing() const {
  return glic_service_->IsPanelShowingForBrowser(*browser_);
}

base::CallbackListSubscription
GlicSplitButtonDelegateImpl::RegisterPanelVisibilityChangedCallback(
    base::RepeatingClosure callback) {
  return glic_service_->instance_coordinator().AddGlobalShowHideCallback(
      std::move(callback));
}

void GlicSplitButtonDelegateImpl::MaybeRecordStartupMetrics() {
  glic_service_->enabling().MaybeRecordStartupMetrics();
}

}  // namespace glic
