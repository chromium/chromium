// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_SAVE_DESK_BUTTON_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_SAVE_DESK_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/style/pill_button.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "base/functional/callback.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class ASH_EXPORT SavedDeskSaveDeskButton : public PillButton {
  METADATA_HEADER(SavedDeskSaveDeskButton, PillButton)

 public:
  SavedDeskSaveDeskButton(base::RepeatingClosure callback,
                          const std::u16string& text,
                          DeskTemplateType type,
                          const gfx::VectorIcon* icon);
  SavedDeskSaveDeskButton(const SavedDeskSaveDeskButton&) = delete;
  SavedDeskSaveDeskButton& operator=(const SavedDeskSaveDeskButton&) = delete;
  ~SavedDeskSaveDeskButton() override;

  DeskTemplateType type() const { return type_; }

 private:
  base::RepeatingClosure callback_;
  DeskTemplateType type_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_SAVED_DESK_SAVE_DESK_BUTTON_H_
