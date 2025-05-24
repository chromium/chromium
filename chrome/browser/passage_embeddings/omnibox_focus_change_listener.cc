// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/passage_embeddings/omnibox_focus_change_listener.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/views/view.h"

SingleOmniboxFocusChangeListener::SingleOmniboxFocusChangeListener(
    views::View* omnibox_view,
    OmniboxFocusChangedCallback focus_changed_callback)
    : omnibox_view_(omnibox_view),
      focus_changed_callback_(focus_changed_callback) {
  observation_.Observe(omnibox_view_->GetWidget()->GetFocusManager());
}

SingleOmniboxFocusChangeListener::~SingleOmniboxFocusChangeListener() = default;

void SingleOmniboxFocusChangeListener::OnDidChangeFocus(
    views::View* focused_before,
    views::View* focused_now) {
  if (focused_before == omnibox_view_ || focused_now == omnibox_view_) {
    DCHECK_NE(focused_before, focused_now);
    focus_changed_callback_.Run(focused_now == omnibox_view_);
  }
}

OmniboxFocusChangedListener::OmniboxFocusChangedListener(
    OmniboxFocusChangedCallback focus_changed_callback)
    : focus_changed_callback_(std::move(focus_changed_callback)) {
  BrowserList::GetInstance()->ForEachCurrentBrowser(
      [this](Browser* browser) { OnBrowserAdded(browser); });
  browser_list_observation_.Observe(BrowserList::GetInstance());
}

OmniboxFocusChangedListener::~OmniboxFocusChangedListener() = default;

void OmniboxFocusChangedListener::OnBrowserAdded(Browser* browser) {
  views::View* const omnibox_view =
      browser->GetBrowserView().toolbar()->location_bar()->omnibox_view();

  single_omnibox_focus_change_listeners_.emplace(
      omnibox_view, std::make_unique<SingleOmniboxFocusChangeListener>(
                        omnibox_view, focus_changed_callback_));
  omnibox_view_observation_.AddObservation(omnibox_view);
}

void OmniboxFocusChangedListener::OnViewIsDeleting(views::View* observed_view) {
  omnibox_view_observation_.RemoveObservation(observed_view);
  single_omnibox_focus_change_listeners_.erase(observed_view);
}
