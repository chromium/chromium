// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_HEADER_H_
#define ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_HEADER_H_

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {

class PillButton;

// The header view shown at the top of the `QuickSettingsView`. Contains an
// optional "Managed by" button and an optional release channel indicator.
class ASH_EXPORT QuickSettingsHeader : public views::View {
 public:
  METADATA_HEADER(QuickSettingsHeader);

  QuickSettingsHeader();
  QuickSettingsHeader(const QuickSettingsHeader&) = delete;
  QuickSettingsHeader& operator=(const QuickSettingsHeader&) = delete;
  ~QuickSettingsHeader() override;

 private:
  // TODO(b/251724754): Remove this temporary placeholder.
  PillButton* placeholder_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_HEADER_H_
