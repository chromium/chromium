// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_helper.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/uuid.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace glic {

class GlicInstanceHelperTest : public testing::Test {
 public:
  GlicInstanceHelperTest() = default;
  ~GlicInstanceHelperTest() override = default;

 protected:
  tabs::MockTabInterface mock_tab_;
  base::HistogramTester histogram_tester_;
  ui::UnownedUserDataHost unowned_user_data_host_;
};

TEST_F(GlicInstanceHelperTest, DestructionLogsNothingIfEmpty) {
  // Scope to ensure destruction afterwards.
  {
    EXPECT_CALL(mock_tab_, GetUnownedUserDataHost())
        .WillRepeatedly(testing::ReturnRef(unowned_user_data_host_));
    GlicInstanceHelper helper(&mock_tab_);
  }
  histogram_tester_.ExpectTotalCount("Glic.Tab.InstanceBindCount", 0);
  histogram_tester_.ExpectTotalCount("Glic.Tab.InstancePinCount", 0);
}

TEST_F(GlicInstanceHelperTest, LogsBindCount) {
  {
    EXPECT_CALL(mock_tab_, GetUnownedUserDataHost())
        .WillRepeatedly(testing::ReturnRef(unowned_user_data_host_));
    GlicInstanceHelper helper(&mock_tab_);
    InstanceId id1 = base::Uuid::GenerateRandomV4();
    InstanceId id2 = base::Uuid::GenerateRandomV4();
    helper.SetInstanceId(id1);
    helper.SetInstanceId(id2);
    // Duplicate should be ignored in count
    helper.SetInstanceId(id1);
  }
  histogram_tester_.ExpectUniqueSample("Glic.Tab.InstanceBindCount", 2, 1);
}

TEST_F(GlicInstanceHelperTest, LogsPinCount) {
  {
    EXPECT_CALL(mock_tab_, GetUnownedUserDataHost())
        .WillRepeatedly(testing::ReturnRef(unowned_user_data_host_));
    GlicInstanceHelper helper(&mock_tab_);
    helper.OnPinnedByInstance(base::Uuid::GenerateRandomV4());
  }
  histogram_tester_.ExpectUniqueSample("Glic.Tab.InstancePinCount", 1, 1);
}

}  // namespace glic
