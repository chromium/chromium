// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/entity_suppression_manager_factory.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/sync/base/features.h"
#include "content/public/test/browser_test.h"

namespace {

using autofill::EntityInstance;
using autofill::EntitySuppressionManager;
using autofill::test::GetPassportEntityInstance;

class TwoClientAutofillEntitySuppressionSyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  TwoClientAutofillEntitySuppressionSyncTest() : SyncTest(TWO_CLIENT) {
    feature_list_.InitWithFeatures(
        {syncer::kSyncAutofillEntitySuppression,
         autofill::features::kAutofillAmbientAutofillSuppression},
        {});
  }

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

  EntitySuppressionManager* GetSuppressionManager(int index) {
    return autofill::EntitySuppressionManagerFactory::GetForProfile(
        GetProfile(index));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         TwoClientAutofillEntitySuppressionSyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(TwoClientAutofillEntitySuppressionSyncTest,
                       SuppressionIsSyncedToOtherClient) {
  const EntityInstance passport = GetPassportEntityInstance();
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(GetSuppressionManager(0)->SuppressEntity(passport));
  ASSERT_TRUE(AwaitQuiescence());

  EXPECT_TRUE(GetSuppressionManager(1)->IsSuppressed(passport));
}

IN_PROC_BROWSER_TEST_P(TwoClientAutofillEntitySuppressionSyncTest,
                       UndoIsSyncedToOtherClient) {
  const EntityInstance passport = GetPassportEntityInstance();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSuppressionManager(0)->SuppressEntity(passport));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(GetSuppressionManager(1)->IsSuppressed(passport));

  ASSERT_TRUE(
      GetSuppressionManager(0)->UndoInSessionSuppressedEntity(passport.guid()));
  ASSERT_TRUE(AwaitQuiescence());

  EXPECT_FALSE(GetSuppressionManager(1)->IsSuppressed(passport));
}

}  // namespace
