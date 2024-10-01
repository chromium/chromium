// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_MANAGEMENT_DISCLOSURE_DIALOG_H_
#define ASH_LOGIN_UI_MANAGEMENT_DISCLOSURE_DIALOG_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/login/ui/non_accessible_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/bulleted_label_list/bulleted_label_list_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

// Implements a window that displays the current device disclosures.
class ASH_EXPORT ManagementDisclosureDialog : public views::DialogDelegateView {
  METADATA_HEADER(ManagementDisclosureDialog, views::DialogDelegateView)

 public:
  using OnManagementDisclosureDismissed = base::RepeatingClosure;
  explicit ManagementDisclosureDialog(
      const std::vector<std::u16string> disclosures,
      base::OnceClosure on_dismissed_callback);

  ManagementDisclosureDialog(const ManagementDisclosureDialog&) = delete;
  ManagementDisclosureDialog& operator=(const ManagementDisclosureDialog&) =
      delete;

  ~ManagementDisclosureDialog() override;

  static gfx::Size GetPreferredSize();

  base::WeakPtr<ManagementDisclosureDialog> GetWeakPtr();

 private:
  base::WeakPtrFactory<ManagementDisclosureDialog> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_MANAGEMENT_DISCLOSURE_DIALOG_H_
