// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_PINE_TEST_API_H_
#define ASH_WM_WINDOW_RESTORE_PINE_TEST_API_H_

#include <memory>

#include "base/memory/raw_ptr.h"

namespace ash {

struct PineContentsData;
class SystemDialogDelegateView;

class PineTestApi {
 public:
  explicit PineTestApi();
  PineTestApi(const PineTestApi&) = delete;
  PineTestApi& operator=(const PineTestApi&) = delete;
  ~PineTestApi();

  void SetPineContentsDataForTesting(
      std::unique_ptr<PineContentsData> pine_contents_data);

  SystemDialogDelegateView* GetOnboardingDialog();
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_PINE_TEST_API_H_
