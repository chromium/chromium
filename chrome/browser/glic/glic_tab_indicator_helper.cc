// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_tab_indicator_helper.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"

namespace glic {

GlicTabIndicatorHelper::GlicTabIndicatorHelper(tabs::TabInterface* tab)
    : tab_(tab) {
  auto* const service = glic::GlicKeyedServiceFactory::GetGlicKeyedService(
      tab_->GetBrowserWindowInterface()->GetProfile());
  focus_change_subscription_ = service->AddFocusedTabChangedCallback(
      base::BindRepeating(&GlicTabIndicatorHelper::OnFocusedTabChanged,
                          base::Unretained(this)));
  indicator_change_subscription_ =
      service->AddContextAccessIndicatorStatusChangedCallback(
          base::BindRepeating(&GlicTabIndicatorHelper::OnIndicatorStatusChanged,
                              base::Unretained(this)));
  will_detach_subscription_ = tab_->RegisterWillDetach(base::BindRepeating(
      &GlicTabIndicatorHelper::OnTabWillDetach, base::Unretained(this)));
  did_insert_subscription_ = tab_->RegisterDidInsert(base::BindRepeating(
      &GlicTabIndicatorHelper::OnTabDidInsert, base::Unretained(this)));
}

GlicTabIndicatorHelper::~GlicTabIndicatorHelper() = default;

void GlicTabIndicatorHelper::MaybeUpdateTab(
    const content::WebContents* contents) {
  if (!contents) {
    return;
  }
  auto* const model = tab_->GetBrowserWindowInterface()->GetTabStripModel();
  CHECK(model);
  const int index = model->GetIndexOfWebContents(contents);
  if (index == TabStripModel::kNoTab) {
    return;
  }
  model->UpdateWebContentsStateAt(index, TabChangeType::kAll);
}

void GlicTabIndicatorHelper::OnFocusedTabChanged(
    const content::WebContents* contents) {
  if (is_detached_) {
    return;
  }

  // TODO(crbug.com/388595318): Simplify this once focus manager changes are
  // finalized.
  MaybeUpdateTab(last_focused_tab_.get());
  MaybeUpdateTab(contents);
  if (contents) {
    // GetWeakPtr() isn't const, but we store a const pointer, so this is
    // safe.
    last_focused_tab_ =
        const_cast<content::WebContents*>(contents)->GetWeakPtr();
  } else {
    last_focused_tab_.reset();
  }
}

void GlicTabIndicatorHelper::OnIndicatorStatusChanged(bool enabled) {
  if (context_access_indicator_enabled_ == enabled) {
    return;
  }
  context_access_indicator_enabled_ = enabled;
  MaybeUpdateTab(last_focused_tab_.get());
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
