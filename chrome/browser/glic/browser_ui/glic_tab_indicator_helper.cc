// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_tab_indicator_helper.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace glic {
DEFINE_USER_DATA(GlicTabIndicatorHelper);

GlicTabIndicatorHelper* GlicTabIndicatorHelper::From(tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

GlicTabIndicatorHelper::GlicTabIndicatorHelper(tabs::TabInterface* tab)
    : tab_(tab),
      glic_keyed_service_(GlicKeyedServiceFactory::GetGlicKeyedService(
          tab_->GetBrowserWindowInterface()->GetProfile())),
      scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this) {
  subscriptions_.push_back(
      glic_keyed_service_->sharing_manager().AddFocusedTabChangedCallback(
          base::BindRepeating(&GlicTabIndicatorHelper::OnFocusedTabChanged,
                              base::Unretained(this))));
  subscriptions_.push_back(
      glic_keyed_service_->AddContextAccessIndicatorStatusChangedCallback(
          base::BindRepeating(&GlicTabIndicatorHelper::OnIndicatorStatusChanged,
                              base::Unretained(this))));
  subscriptions_.push_back(
      glic_keyed_service_->sharing_manager().AddTabPinningStatusChangedCallback(
          base::BindRepeating(
              &GlicTabIndicatorHelper::OnTabPinningStatusChanged,
              base::Unretained(this))));

  // TODO(crbug.com/393525654): This code should not be necessary.
  subscriptions_.push_back(tab_->RegisterWillDetach(base::BindRepeating(
      &GlicTabIndicatorHelper::OnTabWillDetach, base::Unretained(this))));
  subscriptions_.push_back(tab_->RegisterDidInsert(base::BindRepeating(
      &GlicTabIndicatorHelper::OnTabDidInsert, base::Unretained(this))));
}

GlicTabIndicatorHelper::~GlicTabIndicatorHelper() = default;

base::CallbackListSubscription
GlicTabIndicatorHelper::RegisterGlicAccessingStateChange(
    GlicAlertStateChangeCallbackList::CallbackType accessing_change_callback) {
  return glic_accessing_change_callbacks_.Add(
      std::move(accessing_change_callback));
}

base::CallbackListSubscription
GlicTabIndicatorHelper::RegisterGlicSharingStateChange(
    GlicAlertStateChangeCallbackList::CallbackType sharing_change_callback) {
  return glic_sharing_change_callbacks_.Add(std::move(sharing_change_callback));
}

void GlicTabIndicatorHelper::UpdateTab() {
  if (is_detached_) {
    return;
  }

  const bool is_glic_sharing =
      glic_keyed_service_->sharing_manager().IsTabPinned(tab_->GetHandle());
  const bool is_glic_accessing =
      glic_keyed_service_->IsContextAccessIndicatorShown(tab_->GetContents());

  if (is_glic_sharing_ != is_glic_sharing) {
    is_glic_sharing_ = is_glic_sharing;
    glic_sharing_change_callbacks_.Notify(is_glic_sharing_);
  }

  if (is_glic_accessing_ != is_glic_accessing) {
    is_glic_accessing_ = is_glic_accessing;
    glic_accessing_change_callbacks_.Notify(is_glic_accessing_);
  }

  // TODO(crbug.com/422748580): The model should not be notified when the alert
  // state changes after all clients that cares about tab alerts subscribe to
  // the TabAlertController.
  auto* const model = tab_->GetBrowserWindowInterface()->GetTabStripModel();
  const int index = model->GetIndexOfTab(tab_);
  model->UpdateWebContentsStateAt(index, TabChangeType::kAll);
}

void GlicTabIndicatorHelper::OnFocusedTabChanged(
    const FocusedTabData& focused_tab_data) {
  const content::WebContents* contents =
      focused_tab_data.focus() ? focused_tab_data.focus()->GetContents()
                               : nullptr;
  if (tab_is_focused_ && contents != tab_->GetContents()) {
    tab_is_focused_ = false;
    UpdateTab();
    return;
  }

  if (!tab_is_focused_ && contents == tab_->GetContents()) {
    tab_is_focused_ = true;
    UpdateTab();
    return;
  }
}

void GlicTabIndicatorHelper::OnIndicatorStatusChanged(bool enabled) {
  bool is_pinned =
      glic_keyed_service_->sharing_manager().IsTabPinned(tab_->GetHandle());
  if (tab_is_focused_ || is_pinned) {
    UpdateTab();
  }
}

void GlicTabIndicatorHelper::OnTabPinningStatusChanged(tabs::TabInterface* tab,
                                                       bool pinned) {
  if (tab == tab_) {
    UpdateTab();
  }
}

void GlicTabIndicatorHelper::OnTabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  is_detached_ = true;
}

void GlicTabIndicatorHelper::OnTabDidInsert(tabs::TabInterface* tab) {
  is_detached_ = false;
}

}  // namespace glic
