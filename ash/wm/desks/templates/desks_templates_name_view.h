// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_NAME_VIEW_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_NAME_VIEW_H_

#include "ash/wm/desks/desks_textfield.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// Defines a textfield styled to normally look like a label. Allows modifying
// the name of its corresponding template.
class DesksTemplatesNameView : public DesksTextfield {
 public:
  METADATA_HEADER(DesksTemplatesNameView);

  DesksTemplatesNameView();
  DesksTemplatesNameView(const DesksTemplatesNameView&) = delete;
  DesksTemplatesNameView& operator=(const DesksTemplatesNameView&) = delete;
  ~DesksTemplatesNameView() override;

  // Commits an on-going template name change (if any) by bluring the focus away
  // from any view on `widget`, where `widget` should be the desks templates
  // grid widget.
  static void CommitChanges(views::Widget* widget);

  // DesksTextfield:
  void SetTextAndElideIfNeeded(const std::u16string& text) override;

 private:
  friend class DesksTemplatesNameViewTestApi;
};

BEGIN_VIEW_BUILDER(/* no export */, DesksTemplatesNameView, DesksTextfield)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::DesksTemplatesNameView)

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_NAME_VIEW_H_
