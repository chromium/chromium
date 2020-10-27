// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_H_

#include "ash/ash_export.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

// TODO(crbug.com/1142572): Implement content forward icon w/ motion spec.
// The icon used to represent holding space in its tray in the shelf.
class ASH_EXPORT HoldingSpaceTrayIcon : public views::View {
 public:
  METADATA_HEADER(HoldingSpaceTrayIcon);

  HoldingSpaceTrayIcon();
  HoldingSpaceTrayIcon(const HoldingSpaceTrayIcon&) = delete;
  HoldingSpaceTrayIcon& operator=(const HoldingSpaceTrayIcon&) = delete;
  ~HoldingSpaceTrayIcon() override;

  // Returns the preferred main axis margin for this view.
  int GetPreferredMainAxisMargin() const;

  // Invoked when the system locale has changed.
  void OnLocaleChanged();

 private:
  // views::View:
  base::string16 GetTooltipText(const gfx::Point& point) const override;

  void InitLayout();
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_H_
