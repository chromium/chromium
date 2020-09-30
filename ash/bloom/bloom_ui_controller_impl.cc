// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/bloom/bloom_ui_controller_impl.h"

#include "ash/bloom/bloom_ui_delegate_impl.h"

namespace ash {

BloomUiControllerImpl::BloomUiControllerImpl()
    : ui_delegate_(std::make_unique<BloomUiDelegateImpl>()) {}

BloomUiControllerImpl::~BloomUiControllerImpl() = default;

chromeos::bloom::BloomUiDelegate& BloomUiControllerImpl::GetUiDelegate() {
  return *ui_delegate_;
}

}  // namespace ash
