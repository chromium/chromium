// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_border_view_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/border_view.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"

namespace glic {

// Manages the application of the border view UI effect. If focus exists and
// client has enabled the context access indicator then the border is applied to
// the window that contains the focused tab.
GlicBorderViewManager::GlicBorderViewManager(BrowserWindowInterface* browser)
    : browser_(browser) {
  glic_service_ = glic::GlicKeyedServiceFactory::GetGlicKeyedService(
      browser_->GetProfile());

  // Subscribe to changes in the focus tab.
  focus_change_subscription_ =
      glic_service_->AddFocusedTabChangedCallback(base::BindRepeating(
          &GlicBorderViewManager::OnFocusedTabChanged, base::Unretained(this)));
  // Subscribe to changes in the context access indicator status.
  indicator_change_subscription_ =
      glic_service_->AddContextAccessIndicatorStatusChangedCallback(
          base::BindRepeating(&GlicBorderViewManager::OnIndicatorStatusChanged,
                              base::Unretained(this)));

  // Subscribe to active tab changes in this browser window.
  active_tab_change_subscription_ =
      browser_->RegisterActiveTabDidChange(base::BindRepeating(
          &GlicBorderViewManager::OnActiveTabChanged, base::Unretained(this)));
}

GlicBorderViewManager::~GlicBorderViewManager() = default;

void GlicBorderViewManager::UpdateBorderView() {
  BorderView::CancelAllAnimationsForProfile(
      Profile::FromBrowserContext(browser_->GetProfile()));
  auto* const model = browser_->GetTabStripModel();
  CHECK(model);
  const int index = model->GetIndexOfWebContents(focused_tab_.get());

  if (focused_tab_ && glic_service_->window_controller().HasWindow() &&
      index != TabStripModel::kNoTab && context_access_indicator_enabled_) {
    if (BorderView* border =
            BorderView::FindBorderForWebContents(focused_tab_.get())) {
      border->StartAnimation();
    }
  }
}

void GlicBorderViewManager::OnFocusedTabChanged(
    const content::WebContents* contents) {
  if (contents) {
    focused_tab_ = const_cast<content::WebContents*>(contents)->GetWeakPtr();
  } else {
    focused_tab_.reset();
  }
  UpdateBorderView();
}

void GlicBorderViewManager::OnIndicatorStatusChanged(bool enabled) {
  if (context_access_indicator_enabled_ == enabled) {
    return;
  }
  context_access_indicator_enabled_ = enabled;
  UpdateBorderView();
}

void GlicBorderViewManager::OnActiveTabChanged(
    BrowserWindowInterface* browser) {
  CHECK(browser == browser_);
  UpdateBorderView();
}

}  // namespace glic
