// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/data_sharing/public/features.h"
#include "components/sync/base/data_type.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif

namespace {

class SingleClientCollaborationGroupSyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  SingleClientCollaborationGroupSyncTest() : SyncTest(SINGLE_CLIENT) {
    std::vector<base::test::FeatureRef> enabled_features = {
        data_sharing::features::kDataSharingFeature};
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      enabled_features.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
    }
    feature_list_.InitWithFeatures(enabled_features, {});
  }

  ~SingleClientCollaborationGroupSyncTest() override = default;

  void SetUp() override {
#if BUILDFLAG(IS_ANDROID)
    if (base::android::device_info::is_automotive()) {
      // TODO(crbug.com/399444939): Re-enable once automotive is supported.
      GTEST_SKIP() << "Test shouldn't run on automotive builders.";
    }
#endif
  SyncTest::SetUp();
  }

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SingleClientCollaborationGroupSyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(SingleClientCollaborationGroupSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::COLLABORATION_GROUP));
}

}  // namespace
