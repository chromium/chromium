// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multistep_filter/multistep_filter_ui_delegate_impl.h"

#include <utility>

#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"
#include "components/tabs/public/tab_interface.h"
#include "url/gurl.h"

namespace multistep_filter {

MultistepFilterUiDelegateImpl::MultistepFilterUiDelegateImpl(
    tabs::TabInterface& tab)
    : tab_(tab) {}

MultistepFilterUiDelegateImpl::~MultistepFilterUiDelegateImpl() = default;

void MultistepFilterUiDelegateImpl::ClearSuggestion() {
  // Cancel any pending suggestion generation callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (FilterUiController* controller = GetController()) {
    controller->ClearSuggestion();
  }
}

void MultistepFilterUiDelegateImpl::OnSuggestionGenerated(
    std::optional<UrlFilterSuggestion> suggestion) {
  if (FilterUiController* controller = GetController()) {
    controller->OnSuggestionGenerated(std::move(suggestion));
  }
}

bool MultistepFilterUiDelegateImpl::ShouldSuppressSuggestions(
    const GURL& url) const {
  if (const FilterUiController* controller = GetController()) {
    return controller->ShouldSuppressSuggestions(url);
  }
  return false;
}

base::WeakPtr<MultistepFilterUiDelegate>
MultistepFilterUiDelegateImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

FilterUiController* MultistepFilterUiDelegateImpl::GetController() const {
  return FilterUiController::From(&tab_.get());
}

}  // namespace multistep_filter
