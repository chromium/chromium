// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_NAME_VIEW_H_
#define ASH_WM_DESKS_DESK_NAME_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/desks/desks_textfield.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class DeskMiniView;

// Defines a textfield styled to normally look like a label. Allows modifying
// the name of its corresponding desk.
// When Bento is enabled and the user creates a new desk, the accessible name
// for `this` will be the default desk name.
class ASH_EXPORT DeskNameView : public DesksTextfield {
 public:
  METADATA_HEADER(DeskNameView);

  explicit DeskNameView(DeskMiniView* mini_view);
  DeskNameView(const DeskNameView&) = delete;
  DeskNameView& operator=(const DeskNameView&) = delete;
  ~DeskNameView() override;

  // DesksTextfield:
  void OnViewHighlighted() override;

 private:
  // The mini view that associated with this name view.
  DeskMiniView* const mini_view_;
};

BEGIN_VIEW_BUILDER(/* no export */, DeskNameView, DesksTextfield)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::DeskNameView)

#endif  // ASH_WM_DESKS_DESK_NAME_VIEW_H_
