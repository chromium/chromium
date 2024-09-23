// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_MOCK_PICKER_SEARCH_RESULTS_VIEW_DELEGATE_H_
#define ASH_PICKER_VIEWS_MOCK_PICKER_SEARCH_RESULTS_VIEW_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/picker/views/picker_search_results_view_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class ASH_EXPORT MockPickerSearchResultsViewDelegate
    : public PickerSearchResultsViewDelegate {
 public:
  MockPickerSearchResultsViewDelegate();
  ~MockPickerSearchResultsViewDelegate();

  MOCK_METHOD(void, SelectMoreResults, (PickerSectionType), (override));
  MOCK_METHOD(void,
              SelectSearchResult,
              (const PickerSearchResult&),
              (override));
  MOCK_METHOD(void, RequestPseudoFocus, (views::View*), (override));
  MOCK_METHOD(PickerActionType,
              GetActionForResult,
              (const PickerSearchResult& result),
              (override));
  MOCK_METHOD(void, OnSearchResultsViewHeightChanged, (), (override));
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_MOCK_PICKER_SEARCH_RESULTS_VIEW_DELEGATE_H_
