// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_INFO_LABEL_H_
#define ASH_SYSTEM_TRAY_TRAY_INFO_LABEL_H_

#include "ash/ash_export.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace ash {

// A view containing only a label, which is to be inserted as a
// row within a system menu detailed view (e.g., the "Scanning for devices..."
// message that can appear at the top of the Bluetooth detailed view).
class ASH_EXPORT TrayInfoLabel : public views::View {
 public:
  explicit TrayInfoLabel(int message_id);

  TrayInfoLabel(const TrayInfoLabel&) = delete;
  TrayInfoLabel& operator=(const TrayInfoLabel&) = delete;

  ~TrayInfoLabel() override;

  // Updates the TrayInfoLabel to display the message associated with
  // |message_id|.
  void Update(int message_id);

  // views::View:
  const char* GetClassName() const override;

  const views::Label* label() { return label_; }

 private:
  views::Label* const label_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_INFO_LABEL_H_
