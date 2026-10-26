// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/autofill/entity_suppression_manager_factory.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "content/public/test/browser_test.h"

namespace {

using autofill::EntityInstance;
using autofill::EntitySuppressionManager;
using autofill::test::GetPassportEntityInstance;

class SingleClientAutofillEntitySuppressionSyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  SingleClientAutofillEntitySuppressionSyncTest() : SyncTest(SINGLE_CLIENT) {
    feature_list_.InitWithFeatures(
        {syncer::kSyncAutofillEntitySuppression,
         autofill::features::kAutofillAmbientAutofillSuppression},
        {});
  }

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

  EntitySuppressionManager* GetSuppressionManager() {
    return autofill::EntitySuppressionManagerFactory::GetForProfile(
        GetProfile(0));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SingleClientAutofillEntitySuppressionSyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(SingleClientAutofillEntitySuppressionSyncTest,
                       SuppressionIsCommitted) {
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(
      GetSuppressionManager()->SuppressEntity(GetPassportEntityInstance()));

  EXPECT_TRUE(
      ServerCountMatchStatusChecker(syncer::AUTOFILL_ENTITY_SUPPRESSION, 1)
          .Wait());
}

IN_PROC_BROWSER_TEST_P(SingleClientAutofillEntitySuppressionSyncTest,
                       UndoneSuppressionIsDeleted) {
  const EntityInstance passport = GetPassportEntityInstance();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSuppressionManager()->SuppressEntity(passport));
  ASSERT_TRUE(
      ServerCountMatchStatusChecker(syncer::AUTOFILL_ENTITY_SUPPRESSION, 1)
          .Wait());

  ASSERT_TRUE(
      GetSuppressionManager()->UndoInSessionSuppressedEntity(passport.guid()));

  EXPECT_TRUE(
      ServerCountMatchStatusChecker(syncer::AUTOFILL_ENTITY_SUPPRESSION, 0)
          .Wait());
}

IN_PROC_BROWSER_TEST_P(SingleClientAutofillEntitySuppressionSyncTest,
                       ClearedSuppressionIsDeleted) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      GetSuppressionManager()->SuppressEntity(GetPassportEntityInstance()));
  ASSERT_TRUE(
      ServerCountMatchStatusChecker(syncer::AUTOFILL_ENTITY_SUPPRESSION, 1)
          .Wait());

  ASSERT_TRUE(GetSuppressionManager()->ClearSuppressions(
      /*delete_begin=*/base::Time(), /*delete_end=*/base::Time::Max()));

  EXPECT_TRUE(
      ServerCountMatchStatusChecker(syncer::AUTOFILL_ENTITY_SUPPRESSION, 0)
          .Wait());
}

}  // namespace
