// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_SEARCH_CONTROLLER_H_
#define ASH_PICKER_PICKER_SEARCH_CONTROLLER_H_

#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "ash/picker/model/picker_category.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "base/memory/raw_ref.h"

namespace ash {

class PickerClient;

class ASH_EXPORT PickerSearchController {
 public:
  explicit PickerSearchController(PickerClient* client);
  PickerSearchController(const PickerSearchController&) = delete;
  PickerSearchController& operator=(const PickerSearchController&) = delete;

  void StartSearch(const std::u16string& query,
                   std::optional<PickerCategory> category,
                   PickerViewDelegate::SearchResultsCallback callback);

 private:
  const raw_ref<PickerClient> client_;
};

}  // namespace ash

#endif  // ASH_PICKER_PICKER_SEARCH_CONTROLLER_H_
