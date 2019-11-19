// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_INFO_LABEL_H_
#define ASH_SYSTEM_TRAY_TRAY_INFO_LABEL_H_

#include "ash/ash_export.h"
#include "ash/system/tray/actionable_view.h"
#include "ui/views/controls/label.h"

namespace ash {

// A view containing only a label, which is to be inserted as a
// row within a system menu detailed view (e.g., the "Scanning for devices..."
// message that can appear at the top of the Bluetooth detailed view).
// TrayInfoLabel can be clickable; this property is configured by its delegate.
class ASH_EXPORT TrayInfoLabel : public ActionableView {
 public:
  // A delegate for determining whether or not a TrayInfoLabel is clickable, and
  // handling actions when it is clicked.
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnLabelClicked(int message_id) = 0;
    virtual bool IsLabelClickable(int message_id) const = 0;
  };

  // |delegate| may be null, which results in a TrayInfoLabel which cannot be
  // clicked.
  TrayInfoLabel(Delegate* delegate, int message_id);
  ~TrayInfoLabel() override;

  // Updates the TrayInfoLabel to display the message associated with
  // |message_id|. This may update text styling if the delegate indicates that
  // the TrayInfoLabel should be clickable.
  void Update(int message_id);

  // ActionableView:
  bool PerformAction(const ui::Event& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::View:
  const char* GetClassName() const override;

 private:
  friend class TrayInfoLabelTest;

  bool IsClickable();

  views::Label* const label_;
  int message_id_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(TrayInfoLabel);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_INFO_LABEL_H_
