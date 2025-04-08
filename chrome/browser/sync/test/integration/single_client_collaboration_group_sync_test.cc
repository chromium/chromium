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
#include "base/android/build_info.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

class SingleClientCollaborationGroupSyncTest : public SyncTest {
 public:
  SingleClientCollaborationGroupSyncTest() : SyncTest(SINGLE_CLIENT) {
    feature_list_.InitAndEnableFeature(
        data_sharing::features::kDataSharingFeature);
  }

  ~SingleClientCollaborationGroupSyncTest() override = default;

  void SetUp() override {
#if BUILDFLAG(IS_ANDROID)
    if (base::android::BuildInfo::GetInstance()->is_automotive()) {
      // TODO(crbug.com/399444939): Re-enable once automotive is supported.
      GTEST_SKIP() << "Test shouldn't run on automotive builders.";
    }
#endif
  SyncTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SingleClientCollaborationGroupSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::COLLABORATION_GROUP));
}

}  // namespace
