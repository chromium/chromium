// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_MODEL_PICKER_MODEL_H_
#define ASH_PICKER_MODEL_PICKER_MODEL_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/picker/picker_category.h"

namespace ui {
class TextInputClient;
}

namespace ash {

class ASH_EXPORT PickerModel {
 public:
  // `focused_client` is the input field that was focused when Picker is opened.
  explicit PickerModel(ui::TextInputClient* focused_client = nullptr);

  std::vector<PickerCategory> GetAvailableCategories() const;

 private:
  bool has_selected_text_;
};

}  // namespace ash

#endif  // ASH_PICKER_MODEL_PICKER_MODEL_H_
