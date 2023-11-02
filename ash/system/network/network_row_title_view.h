// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_ROW_TITLE_VIEW_H_
#define ASH_SYSTEM_NETWORK_NETWORK_ROW_TITLE_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/controls/label.h"

namespace ash {

// Title row for the network section of quick settings, which displays the name
// of a network type (e.g., Wi-Fi or Mobile data).
class ASH_EXPORT NetworkRowTitleView : public views::View {
 public:
  explicit NetworkRowTitleView(int title_message_id);

  NetworkRowTitleView(const NetworkRowTitleView&) = delete;
  NetworkRowTitleView& operator=(const NetworkRowTitleView&) = delete;

  ~NetworkRowTitleView() override;

  // views::View:
  const char* GetClassName() const override;

 private:
  views::Label* const title_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_ROW_TITLE_VIEW_H_
