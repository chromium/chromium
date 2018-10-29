// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CAPTION_BUTTONS_CAPTION_BUTTON_MODEL_H_
#define ASH_PUBLIC_CPP_CAPTION_BUTTONS_CAPTION_BUTTON_MODEL_H_

#include "ash/public/cpp/caption_buttons/caption_button_types.h"

namespace ash {

// CaptionButtonModel describes the state of caption buttons
// for each CaptionButtonIcon types.
class CaptionButtonModel {
 public:
  virtual ~CaptionButtonModel() {}

  // Returns true if |type| is visible.
  virtual bool IsVisible(CaptionButtonIcon type) const = 0;

  // Returns true if |type| is enabled.
  virtual bool IsEnabled(CaptionButtonIcon type) const = 0;

  // In zoom mode, the maximize/restore button will be repalced
  // with zoom/unzoom button.
  virtual bool InZoomMode() const = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CAPTION_BUTTONS_CAPTION_BUTTON_MODEL_H_
