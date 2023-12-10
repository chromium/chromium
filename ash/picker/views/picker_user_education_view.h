// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_USER_EDUCATION_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_USER_EDUCATION_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {

// A view that educates the user about the Picker feature.
// Shows a list of key bindings that the user can use.
class ASH_EXPORT PickerUserEducationView : public views::View {
 public:
  METADATA_HEADER(PickerUserEducationView);

  PickerUserEducationView();
  PickerUserEducationView(const PickerUserEducationView&) = delete;
  PickerUserEducationView& operator=(const PickerUserEducationView&) = delete;
  ~PickerUserEducationView() override;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_USER_EDUCATION_VIEW_H_
