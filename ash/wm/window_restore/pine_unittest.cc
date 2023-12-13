// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_contents_view.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_restore/window_restore_controller.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class PineTest : public AshTestBase {
 public:
  PineTest() = default;
  PineTest(const PineTest&) = delete;
  PineTest& operator=(const PineTest&) = delete;
  ~PineTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kPine};
};

TEST_F(PineTest, Show) {
  Shell::Get()->window_restore_controller()->MaybeStartInformedRestore();
}

}  // namespace ash
