// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_MODEL_PICKER_MODEL_H_
#define ASH_PICKER_MODEL_PICKER_MODEL_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/picker/model/picker_category.h"

namespace ash {

class ASH_EXPORT PickerModel {
 public:
  std::vector<PickerCategory> GetAvailableCategories() const;
};

}  // namespace ash

#endif  // ASH_PICKER_MODEL_PICKER_MODEL_H_
