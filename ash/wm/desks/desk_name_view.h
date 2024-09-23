// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_NAME_VIEW_H_
#define ASH_WM_DESKS_DESK_NAME_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/desks/desk_textfield.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class DeskMiniView;

// Defines a textfield styled to normally look like a label. Allows modifying
// the name of its corresponding desk. The accessible name for `this` will be
// the default desk name.
class ASH_EXPORT DeskNameView : public DeskTextfield {
  METADATA_HEADER(DeskNameView, DeskTextfield)

 public:
  explicit DeskNameView(DeskMiniView* mini_view);
  DeskNameView(const DeskNameView&) = delete;
  DeskNameView& operator=(const DeskNameView&) = delete;
  ~DeskNameView() override;

  // DeskTextfield:
  void OnFocus() override;

 private:
  // The mini view that associated with this name view.
  const raw_ptr<DeskMiniView> mini_view_;
};

BEGIN_VIEW_BUILDER(/* no export */, DeskNameView, DeskTextfield)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::DeskNameView)

#endif  // ASH_WM_DESKS_DESK_NAME_VIEW_H_
