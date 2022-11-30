// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_button_delegate.h"

namespace ash {

std::unique_ptr<ShelfButtonDelegate::ScopedActiveInkDropCount>
ShelfButtonDelegate::CreateScopedActiveInkDropCount(const ShelfButton* sender) {
  return nullptr;
}

}  // namespace ash
