// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_tab_indicator_helper.h"

#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"

namespace glic {

GlicTabIndicatorHelper::GlicTabIndicatorHelper(BrowserWindowInterface* browser)
    : browser_(*browser) {
  auto* const service = glic::GlicKeyedServiceFactory::GetGlicKeyedService(
      browser_->GetProfile());
  SetLastFocusedTab(service->GetFocusedTab());
  change_subscription_ = service->AddFocusedTabChangedCallback(
      base::BindRepeating(&GlicTabIndicatorHelper::OnFocusedTabChanged,
                          base::Unretained(this)));
}

GlicTabIndicatorHelper::~GlicTabIndicatorHelper() = default;

void GlicTabIndicatorHelper::SetLastFocusedTab(
    const content::WebContents* contents) {
  if (contents) {
    // GetWeakPtr() isn't const, but we store a const pointer, so this is
    // safe.
    last_focused_tab_ =
        const_cast<content::WebContents*>(contents)->GetWeakPtr();
  } else {
    last_focused_tab_.reset();
  }
}

void GlicTabIndicatorHelper::OnFocusedTabChanged(
    const content::WebContents* contents) {
  if (contents == last_focused_tab_.get()) {
    return;
  }

  MaybeUpdateTab(last_focused_tab_.get());
  MaybeUpdateTab(contents);
  SetLastFocusedTab(contents);
}

// Possibly sends an update for the renderer data for the given tab.
void GlicTabIndicatorHelper::MaybeUpdateTab(
    const content::WebContents* contents) {
  if (!contents) {
    return;
  }
  auto* const model = browser_->GetTabStripModel();
  CHECK(model);
  const int index = model->GetIndexOfWebContents(contents);
  if (index == TabStripModel::kNoTab) {
    return;
  }
  model->UpdateWebContentsStateAt(index, TabChangeType::kAll);
}

}  // namespace glic
