// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONTENT_SHIMLESS_RMA_SHIMLESS_RMA_H_
#define ASH_CONTENT_SHIMLESS_RMA_SHIMLESS_RMA_H_

#include "ui/web_dialogs/web_dialog_ui.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash {

// The WebUI for ShimlessRMA or chrome://shimless-rma.
class ShimlessRMADialogUI : public ui::MojoWebDialogUI {
 public:
  explicit ShimlessRMADialogUI(content::WebUI* web_ui);
  ~ShimlessRMADialogUI() override;

  ShimlessRMADialogUI(const ShimlessRMADialogUI&) = delete;
  ShimlessRMADialogUI& operator=(const ShimlessRMADialogUI&) = delete;
};

}  // namespace ash

#endif  // ASH_CONTENT_SHIMLESS_RMA_SHIMLESS_RMA_H_
