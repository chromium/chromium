// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_DIALOG_H_
#define ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_DIALOG_H_

#include <memory>

#include "ash/style/system_dialog_delegate_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/widget/widget.h"

namespace aura {
class Window;
}

namespace ash {

class InformedRestoreDialog : public SystemDialogDelegateView {
 public:
  METADATA_HEADER(InformedRestoreDialog);

  using AppIds = std::vector<std::string>;

  InformedRestoreDialog(const InformedRestoreDialog&) = delete;
  InformedRestoreDialog& operator=(const InformedRestoreDialog&) = delete;
  ~InformedRestoreDialog() override;

  static std::unique_ptr<views::Widget> Create(aura::Window* root);

 private:
  explicit InformedRestoreDialog(const AppIds& app_ids);
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_DIALOG_H_
