// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_TEST_BASE_H_
#define ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_TEST_BASE_H_

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class InformedRestoreTestBase : public AshTestBase {
 public:
  InformedRestoreTestBase();
  InformedRestoreTestBase(const InformedRestoreTestBase&) = delete;
  InformedRestoreTestBase& operator=(const InformedRestoreTestBase&) = delete;
  ~InformedRestoreTestBase() override;

  PrefService* GetTestPrefService();

  // AshTestBase:
  void SetUp() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kForestFeature};
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_TEST_BASE_H_
