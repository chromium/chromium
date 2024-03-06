// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_PSEUDO_FOCUS_H_
#define ASH_PICKER_VIEWS_PICKER_PSEUDO_FOCUS_H_

#include "ash/ash_export.h"

namespace views {
class View;
}  // namespace views

namespace ash {

void ASH_EXPORT ApplyPickerPseudoFocusToView(views::View* view);

void ASH_EXPORT RemovePickerPseudoFocusFromView(views::View* view);

bool ASH_EXPORT DoPickerPseudoFocusedActionOnView(views::View* view);

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_PSEUDO_FOCUS_H_
