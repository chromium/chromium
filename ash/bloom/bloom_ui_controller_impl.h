// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BLOOM_BLOOM_UI_CONTROLLER_IMPL_H_
#define ASH_BLOOM_BLOOM_UI_CONTROLLER_IMPL_H_

#include <memory>

#include "chromeos/components/bloom/public/cpp/bloom_ui_controller.h"

namespace ash {

class BloomUiControllerImpl : public chromeos::bloom::BloomUiController {
 public:
  BloomUiControllerImpl();
  ~BloomUiControllerImpl() override;

  // chromeos::bloom::BloomUiController implementation:
  chromeos::bloom::BloomUiDelegate& GetUiDelegate() override;

 private:
  std::unique_ptr<chromeos::bloom::BloomUiDelegate> ui_delegate_;
};

}  // namespace ash

#endif  // ASH_BLOOM_BLOOM_UI_CONTROLLER_IMPL_H_
