// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_VIEW_DELEGATE_H_
#define ASH_PICKER_VIEWS_PICKER_VIEW_DELEGATE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/ash_web_view.h"

namespace ash {

class PickerSearchResult;
class PickerSearchResults;

// Delegate for `PickerView`.
class ASH_EXPORT PickerViewDelegate {
 public:
  using SearchResultsCallback =
      base::RepeatingCallback<void(const PickerSearchResults& results)>;

  virtual ~PickerViewDelegate() {}

  virtual std::unique_ptr<AshWebView> CreateWebView(
      const AshWebView::InitParams& params) = 0;

  // Starts a search for `query`. Results will be returned via `callback`,
  // which may be called multiples times to update the results.
  virtual void StartSearch(const std::u16string& query,
                           SearchResultsCallback callback) = 0;

  // Inserts `result` into the previously focused input field.
  virtual void InsertResult(const PickerSearchResult& result) = 0;

  // Whether the view should paint. Certain test scenarios do not need
  // painting, so it is better to skip painting.
  virtual bool ShouldPaint() = 0;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_VIEW_DELEGATE_H_
