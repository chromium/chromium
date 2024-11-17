// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_UNFOCUSABLE_LABEL_H_
#define ASH_SYSTEM_TRAY_UNFOCUSABLE_LABEL_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/label.h"

namespace ash {

// A label which is not focusable with ChromeVox.
class ASH_EXPORT UnfocusableLabel : public views::Label {
  METADATA_HEADER(UnfocusableLabel, views::Label)

 public:
  UnfocusableLabel();

  UnfocusableLabel(const UnfocusableLabel&) = delete;
  UnfocusableLabel& operator=(const UnfocusableLabel&) = delete;

  ~UnfocusableLabel() override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_UNFOCUSABLE_LABEL_H_
