// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SingleClientSkillsSyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  SingleClientSkillsSyncTest() : SyncTest(SINGLE_CLIENT) {
    std::vector<base::test::FeatureRef> enabled_features = {syncer::kSyncSkill};
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      enabled_features.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
    }
    feature_overrides_.InitWithFeatures(enabled_features, {});
  }

 private:
  base::test::ScopedFeatureList feature_overrides_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SingleClientSkillsSyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(SingleClientSkillsSyncTest, ShouldInitializeDataType) {
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::SKILL));
}

}  // namespace
