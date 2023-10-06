// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_SYSTEM_TOAST_VIEW_H_
#define ASH_SYSTEM_TOAST_SYSTEM_TOAST_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash {

struct ToastData;
class SystemShadow;

// The System Toast view. (go/toast-style-spec)
// This view supports different configurations depending on the provided
// toast data parameters. It will always have a body text, and may have a
// leading icon and a trailing button.
class ASH_EXPORT SystemToastView : public views::FlexLayoutView {
 public:
  METADATA_HEADER(SystemToastView);

  explicit SystemToastView(const ToastData& toast_data);
  SystemToastView(const SystemToastView&) = delete;
  SystemToastView& operator=(const SystemToastView&) = delete;
  ~SystemToastView() override;

 private:
  std::unique_ptr<SystemShadow> shadow_;

  // views::View:
  void AddedToWidget() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_SYSTEM_TOAST_VIEW_H_
