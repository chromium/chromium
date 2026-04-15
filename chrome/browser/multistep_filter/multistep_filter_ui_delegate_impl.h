// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MULTISTEP_FILTER_MULTISTEP_FILTER_UI_DELEGATE_IMPL_H_
#define CHROME_BROWSER_MULTISTEP_FILTER_MULTISTEP_FILTER_UI_DELEGATE_IMPL_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/multistep_filter_ui_delegate.h"

class GURL;

namespace tabs {
class TabInterface;
}

namespace multistep_filter {

class FilterUiController;

class MultistepFilterUiDelegateImpl final : public MultistepFilterUiDelegate {
 public:
  explicit MultistepFilterUiDelegateImpl(tabs::TabInterface& tab);
  ~MultistepFilterUiDelegateImpl() override;

  MultistepFilterUiDelegateImpl(const MultistepFilterUiDelegateImpl&) = delete;
  MultistepFilterUiDelegateImpl& operator=(
      const MultistepFilterUiDelegateImpl&) = delete;

  // MultistepFilterUiDelegate:
  void ClearSuggestion() override;
  void OnSuggestionGenerated(
      std::optional<UrlFilterSuggestion> suggestion) override;
  bool ShouldSuppressSuggestions(const GURL& url) const override;
  base::WeakPtr<MultistepFilterUiDelegate> GetWeakPtr() override;

 private:
  FilterUiController* GetController() const;

  raw_ref<tabs::TabInterface> tab_;

  // This should be kept at the end so that it is the first member to be
  // destroyed.
  base::WeakPtrFactory<MultistepFilterUiDelegateImpl> weak_ptr_factory_{this};
};

}  // namespace multistep_filter

#endif  // CHROME_BROWSER_MULTISTEP_FILTER_MULTISTEP_FILTER_UI_DELEGATE_IMPL_H_
