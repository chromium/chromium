// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PICKER_PICKER_CLIENT_H_
#define ASH_PUBLIC_CPP_PICKER_PICKER_CLIENT_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/ash_web_view.h"

namespace ash {

// Lets PickerController in Ash to communicate with the browser.
class ASH_PUBLIC_EXPORT PickerClient {
 public:
  virtual std::unique_ptr<ash::AshWebView> CreateWebView(
      const ash::AshWebView::InitParams& params) = 0;

 protected:
  PickerClient();
  virtual ~PickerClient();
};

}  // namespace ash

#endif
