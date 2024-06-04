// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_TEST_API_H_
#define ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_TEST_API_H_

#include <memory>

namespace ash {

struct InformedRestoreContentsData;
class SystemDialogDelegateView;

class InformedRestoreTestApi {
 public:
  explicit InformedRestoreTestApi();
  InformedRestoreTestApi(const InformedRestoreTestApi&) = delete;
  InformedRestoreTestApi& operator=(const InformedRestoreTestApi&) = delete;
  ~InformedRestoreTestApi();

  void SetInformedRestoreContentsDataForTesting(
      std::unique_ptr<InformedRestoreContentsData> contents_data);

  SystemDialogDelegateView* GetOnboardingDialog();
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_TEST_API_H_
