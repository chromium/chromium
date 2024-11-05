// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_WIDGET_DELEGATE_H_
#define ASH_SYSTEM_MAHI_MAHI_WIDGET_DELEGATE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

// A custom widget delegate for the Mahi window. Used to inject custom
// dependencies that are instantiated in the base widget class (like the custom
// Mahi frame view).
class ASH_EXPORT MahiWidgetDelegate : public views::WidgetDelegate {
 public:
  MahiWidgetDelegate();
  MahiWidgetDelegate(const MahiWidgetDelegate&) = delete;
  MahiWidgetDelegate& operator=(const MahiWidgetDelegate&) = delete;
  ~MahiWidgetDelegate() override;

  // views::WidgetDelegate:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAHI_MAHI_WIDGET_DELEGATE_H_
