// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/merchant_viewer/merchant_viewer_data_manager.h"

#include <optional>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/ranges/algorithm.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/commerce/merchant_viewer/merchant_viewer_data_manager_factory.h"
#include "chrome/browser/persisted_state_db/session_proto_db_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/session_proto_db/session_proto_db.h"
#include "content/public/browser/android/browser_context_handle.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using std::string;
using std::vector;

namespace {
merchant_signal_db::MerchantSignalContentProto BuildProto(
    const char* hostname,
    base::Time time_created) {
  merchant_signal_db::MerchantSignalContentProto proto;
  proto.set_key(hostname);
  proto.set_trust_signals_message_displayed_timestamp(
      time_created.InSecondsFSinceUnixEpoch());
  return proto;
}
}  // namespace

class MerchantViewerDataManagerTest : public testing::Test {
 public:
  MerchantViewerDataManagerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    testing::Test::SetUp();

    service_ = MerchantViewerDataManagerFactory::GetForProfile(&profile_);
  }

  void TearDown() override {
    if (service_)
      service_->GetDB()->Destroy();
  }

  void OperationEvaluation(base::OnceClosure closure,
                           bool expected_success,
                           bool actual_success) {
    EXPECT_EQ(expected_success, actual_success);
    std::move(closure).Run();
  }

  void LoadEntriesEvaluation(base::OnceClosure closure,
                             vector<string> expected_hostnames,
                             bool success,
                             MerchantViewerDataManager::MerchantSignals found) {
    EXPECT_TRUE(success);

    EXPECT_THAT(base::ToVector(
                    found, [](const auto& item) { return item.second.key(); }),
                testing::UnorderedElementsAreArray(expected_hostnames));

    std::move(closure).Run();
  }

 protected:
  base::test::ScopedFeatureList features_;
  // Required to run tests from UI thread.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  raw_ptr<MerchantViewerDataManager> service_;
};

const char kMockMerchantA[] = "foo.com";
const char kMockMerchantUrlA[] = "https://foo.com/";
const char kMockMerchantB[] = "bar.com";
const char kMockMerchantUrlB[] = "https://bar.com/";

TEST_F(MerchantViewerDataManagerTest, TestDeleteMerchantViewerDataForOrigins) {
  base::HistogramTester histogram_tester;

  SessionProtoDB<merchant_signal_db::MerchantSignalContentProto>* db =
      service_->GetDB();

  merchant_signal_db::MerchantSignalContentProto protoA =
      BuildProto(kMockMerchantA, base::Time::Now());

  merchant_signal_db::MerchantSignalContentProto protoB =
      BuildProto(kMockMerchantB, base::Time::Now());

  base::RunLoop run_loop[4];

  db->InsertContent(
      kMockMerchantA, protoA,
      base::BindOnce(&MerchantViewerDataManagerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  db->InsertContent(
      kMockMerchantB, protoB,
      base::BindOnce(&MerchantViewerDataManagerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[1].QuitClosure(), true));
  run_loop[1].Run();

  vector<string> hostnames_before_deletion = {kMockMerchantA, kMockMerchantB};

  db->LoadAllEntries(
      base::BindOnce(&MerchantViewerDataManagerTest::LoadEntriesEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(),
                     hostnames_before_deletion));
  run_loop[2].Run();

  base::flat_set<GURL> deleted_origins = {GURL(kMockMerchantUrlB)};

  service_->DeleteMerchantViewerDataForOrigins(deleted_origins);
  task_environment_.RunUntilIdle();

  vector<string> hostnames_after_deletion = {kMockMerchantA};
  db->LoadAllEntries(
      base::BindOnce(&MerchantViewerDataManagerTest::LoadEntriesEvaluation,
                     base::Unretained(this), run_loop[3].QuitClosure(),
                     hostnames_after_deletion));
  run_loop[3].Run();

  histogram_tester.ExpectUniqueSample(
      "MerchantViewer.DataManager.DeleteMerchantViewerDataForOrigins", 1, 1);
}

TEST_F(MerchantViewerDataManagerTest,
       TestDeleteMerchantViewerDataForOriginsEmpty) {
  base::HistogramTester histogram_tester;

  SessionProtoDB<merchant_signal_db::MerchantSignalContentProto>* db =
      service_->GetDB();

  merchant_signal_db::MerchantSignalContentProto protoA =
      BuildProto(kMockMerchantA, base::Time::Now());

  base::RunLoop run_loop[3];

  db->InsertContent(
      kMockMerchantA, protoA,
      base::BindOnce(&MerchantViewerDataManagerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();
  vector<string> expected_hostnames = {kMockMerchantA};

  db->LoadAllEntries(base::BindOnce(
      &MerchantViewerDataManagerTest::LoadEntriesEvaluation,
      base::Unretained(this), run_loop[1].QuitClosure(), expected_hostnames));
  run_loop[1].Run();

  base::flat_set<GURL> deleted_origins = {};

  service_->DeleteMerchantViewerDataForOrigins(deleted_origins);
  task_environment_.RunUntilIdle();

  db->LoadAllEntries(base::BindOnce(
      &MerchantViewerDataManagerTest::LoadEntriesEvaluation,
      base::Unretained(this), run_loop[2].QuitClosure(), expected_hostnames));
  run_loop[2].Run();

  histogram_tester.ExpectUniqueSample(
      "MerchantViewer.DataManager.DeleteMerchantViewerDataForOrigins", 0, 1);
}

TEST_F(MerchantViewerDataManagerTest, DeleteMerchantViewerDataForTimeRange) {
  base::HistogramTester histogram_tester;

  SessionProtoDB<merchant_signal_db::MerchantSignalContentProto>* db =
      service_->GetDB();

  base::Time start_time = base::Time::Now();
  base::Time end_time = start_time + base::Days(3);

  merchant_signal_db::MerchantSignalContentProto protoA =
      BuildProto(kMockMerchantA, start_time + base::Days(1));

  merchant_signal_db::MerchantSignalContentProto protoB =
      BuildProto(kMockMerchantB, start_time + base::Days(2));

  base::RunLoop run_loop[4];

  db->InsertContent(
      kMockMerchantA, protoA,
      base::BindOnce(&MerchantViewerDataManagerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  db->InsertContent(
      kMockMerchantB, protoB,
      base::BindOnce(&MerchantViewerDataManagerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[1].QuitClosure(), true));
  run_loop[1].Run();

  vector<string> hostnames_before_deletion = {kMockMerchantA, kMockMerchantB};

  db->LoadAllEntries(
      base::BindOnce(&MerchantViewerDataManagerTest::LoadEntriesEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(),
                     hostnames_before_deletion));
  run_loop[2].Run();

  service_->DeleteMerchantViewerDataForTimeRange(start_time, end_time);
  task_environment_.RunUntilIdle();
  vector<string> hostnames_after_deletion = {};

  db->LoadAllEntries(
      base::BindOnce(&MerchantViewerDataManagerTest::LoadEntriesEvaluation,
                     base::Unretained(this), run_loop[3].QuitClosure(),
                     hostnames_after_deletion));
  run_loop[3].Run();

  histogram_tester.ExpectUniqueSample(
      "MerchantViewer.DataManager.DeleteMerchantViewerDataForTimeRange", 2, 1);
}

TEST_F(MerchantViewerDataManagerTest,
       DeleteMerchantViewerDataForTimeRangeNoDeletion) {
  base::HistogramTester histogram_tester;

  SessionProtoDB<merchant_signal_db::MerchantSignalContentProto>* db =
      service_->GetDB();

  base::Time start_time = base::Time::Now();
  base::Time end_time = start_time + base::Days(3);

  merchant_signal_db::MerchantSignalContentProto protoA =
      BuildProto(kMockMerchantA, start_time - base::Days(1));

  merchant_signal_db::MerchantSignalContentProto protoB =
      BuildProto(kMockMerchantB, start_time - base::Days(1));

  base::RunLoop run_loop[4];

  db->InsertContent(
      kMockMerchantA, protoA,
      base::BindOnce(&MerchantViewerDataManagerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  db->InsertContent(
      kMockMerchantB, protoB,
      base::BindOnce(&MerchantViewerDataManagerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[1].QuitClosure(), true));
  run_loop[1].Run();
  vector<string> expected_hostnames = {kMockMerchantA, kMockMerchantB};

  db->LoadAllEntries(base::BindOnce(
      &MerchantViewerDataManagerTest::LoadEntriesEvaluation,
      base::Unretained(this), run_loop[2].QuitClosure(), expected_hostnames));
  run_loop[2].Run();

  service_->DeleteMerchantViewerDataForTimeRange(start_time, end_time);
  task_environment_.RunUntilIdle();

  db->LoadAllEntries(base::BindOnce(
      &MerchantViewerDataManagerTest::LoadEntriesEvaluation,
      base::Unretained(this), run_loop[3].QuitClosure(), expected_hostnames));
  run_loop[3].Run();

  histogram_tester.ExpectUniqueSample(
      "MerchantViewer.DataManager.DeleteMerchantViewerDataForTimeRange", 0, 1);
}

TEST_F(MerchantViewerDataManagerTest,
       DeleteMerchantViewerDataForTimeRangeWithinWindow) {
  base::HistogramTester histogram_tester;

  SessionProtoDB<merchant_signal_db::MerchantSignalContentProto>* db =
      service_->GetDB();

  base::Time start_time = base::Time::Now();
  base::Time end_time = start_time + base::Days(3);

  merchant_signal_db::MerchantSignalContentProto protoA =
      BuildProto(kMockMerchantA, start_time - base::Days(1));

  merchant_signal_db::MerchantSignalContentProto protoB =
      BuildProto(kMockMerchantB, start_time + base::Days(1));

  base::RunLoop run_loop[4];

  db->InsertContent(
      kMockMerchantA, protoA,
      base::BindOnce(&MerchantViewerDataManagerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  db->InsertContent(
      kMockMerchantB, protoB,
      base::BindOnce(&MerchantViewerDataManagerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[1].QuitClosure(), true));
  run_loop[1].Run();
  vector<string> hostnames_before_deletion = {kMockMerchantA, kMockMerchantB};

  db->LoadAllEntries(
      base::BindOnce(&MerchantViewerDataManagerTest::LoadEntriesEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(),
                     hostnames_before_deletion));
  run_loop[2].Run();

  service_->DeleteMerchantViewerDataForTimeRange(start_time, end_time);
  task_environment_.RunUntilIdle();
  vector<string> hostnames_after_deletion = {kMockMerchantA};

  db->LoadAllEntries(
      base::BindOnce(&MerchantViewerDataManagerTest::LoadEntriesEvaluation,
                     base::Unretained(this), run_loop[3].QuitClosure(),
                     hostnames_after_deletion));
  run_loop[3].Run();

  histogram_tester.ExpectUniqueSample(
      "MerchantViewer.DataManager.DeleteMerchantViewerDataForTimeRange", 1, 1);
}

TEST_F(MerchantViewerDataManagerTest,
       DeleteMerchantViewerDataForOrigins_OriginNotFound) {
  base::HistogramTester histogram_tester;

  SessionProtoDB<merchant_signal_db::MerchantSignalContentProto>* db =
      service_->GetDB();

  base::RunLoop run_loop[1];
  base::flat_set<GURL> deleted_origins = {GURL(kMockMerchantUrlB)};
  service_->DeleteMerchantViewerDataForOrigins(std::move(deleted_origins));
  task_environment_.RunUntilIdle();
  vector<string> expected_hostnames = {};

  db->LoadAllEntries(base::BindOnce(
      &MerchantViewerDataManagerTest::LoadEntriesEvaluation,
      base::Unretained(this), run_loop[0].QuitClosure(), expected_hostnames));
  run_loop[0].Run();
}

TEST_F(MerchantViewerDataManagerTest,
       DeleteMerchantViewerDataForOrigins_VerifyCount) {
  base::HistogramTester histogram_tester;

  SessionProtoDB<merchant_signal_db::MerchantSignalContentProto>* db =
      service_->GetDB();

  merchant_signal_db::MerchantSignalContentProto protoA =
      BuildProto(kMockMerchantA, base::Time::Now());

  merchant_signal_db::MerchantSignalContentProto protoB =
      BuildProto(kMockMerchantB, base::Time::Now());

  base::RunLoop run_loop[4];

  db->InsertContent(
      kMockMerchantA, protoA,
      base::BindOnce(&MerchantViewerDataManagerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();
  db->InsertContent(
      kMockMerchantB, protoB,
      base::BindOnce(&MerchantViewerDataManagerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[1].QuitClosure(), true));
  run_loop[1].Run();
  vector<string> expected_hostnames = {kMockMerchantA, kMockMerchantB};

  db->LoadAllEntries(base::BindOnce(
      &MerchantViewerDataManagerTest::LoadEntriesEvaluation,
      base::Unretained(this), run_loop[2].QuitClosure(), expected_hostnames));
  run_loop[2].Run();

  base::flat_set<GURL> deleted_origins = {GURL(kMockMerchantUrlA),
                                          GURL(kMockMerchantUrlB)};

  service_->DeleteMerchantViewerDataForOrigins(deleted_origins);
  task_environment_.RunUntilIdle();
  vector<string> expected_hostnames_after_deletion = {};

  db->LoadAllEntries(
      base::BindOnce(&MerchantViewerDataManagerTest::LoadEntriesEvaluation,
                     base::Unretained(this), run_loop[3].QuitClosure(),
                     expected_hostnames_after_deletion));
  run_loop[3].Run();

  histogram_tester.ExpectUniqueSample(
      "MerchantViewer.DataManager.DeleteMerchantViewerDataForOrigins", 2, 1);
}

TEST_F(MerchantViewerDataManagerTest,
       DeleteMerchantViewerDataForOrigins_ForceClearAll) {
  features_.InitAndEnableFeatureWithParameters(
      commerce::kCommerceMerchantViewer,
      {{"delete_all_merchants_on_clear_history", "true"}});

  base::HistogramTester histogram_tester;

  SessionProtoDB<merchant_signal_db::MerchantSignalContentProto>* db =
      service_->GetDB();

  merchant_signal_db::MerchantSignalContentProto protoA =
      BuildProto(kMockMerchantA, base::Time::Now());

  merchant_signal_db::MerchantSignalContentProto protoB =
      BuildProto(kMockMerchantB, base::Time::Now());

  base::RunLoop run_loop[4];

  db->InsertContent(
      kMockMerchantA, protoA,
      base::BindOnce(&MerchantViewerDataManagerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();
  db->InsertContent(
      kMockMerchantB, protoB,
      base::BindOnce(&MerchantViewerDataManagerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[1].QuitClosure(), true));
  run_loop[1].Run();
  vector<string> expected_hostnames = {kMockMerchantA, kMockMerchantB};

  db->LoadAllEntries(base::BindOnce(
      &MerchantViewerDataManagerTest::LoadEntriesEvaluation,
      base::Unretained(this), run_loop[2].QuitClosure(), expected_hostnames));
  run_loop[2].Run();

  base::flat_set<GURL> deleted_origins = {GURL(kMockMerchantUrlA)};

  service_->DeleteMerchantViewerDataForOrigins(deleted_origins);
  task_environment_.RunUntilIdle();
  vector<string> expected_hostnames_after_deletion = {};

  db->LoadAllEntries(
      base::BindOnce(&MerchantViewerDataManagerTest::LoadEntriesEvaluation,
                     base::Unretained(this), run_loop[3].QuitClosure(),
                     expected_hostnames_after_deletion));
  run_loop[3].Run();

  histogram_tester.ExpectUniqueSample(
      "MerchantViewer.DataManager.ForceClearMerchantsForOrigins", true, 1);
}

TEST_F(MerchantViewerDataManagerTest,
       DeleteMerchantViewerDataForTimeRange_ForceAll) {
  features_.InitAndEnableFeatureWithParameters(
      commerce::kCommerceMerchantViewer,
      {{"delete_all_merchants_on_clear_history", "true"}});

  base::HistogramTester histogram_tester;

  SessionProtoDB<merchant_signal_db::MerchantSignalContentProto>* db =
      service_->GetDB();

  base::Time start_time = base::Time::Now();
  base::Time end_time = start_time + base::Days(3);

  merchant_signal_db::MerchantSignalContentProto protoA =
      BuildProto(kMockMerchantA, start_time - base::Days(4));

  merchant_signal_db::MerchantSignalContentProto protoB =
      BuildProto(kMockMerchantB, start_time + base::Days(1));

  base::RunLoop run_loop[4];

  db->InsertContent(
      kMockMerchantA, protoA,
      base::BindOnce(&MerchantViewerDataManagerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  db->InsertContent(
      kMockMerchantB, protoB,
      base::BindOnce(&MerchantViewerDataManagerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[1].QuitClosure(), true));
  run_loop[1].Run();
  vector<string> hostnames_before_deletion = {kMockMerchantA, kMockMerchantB};

  db->LoadAllEntries(
      base::BindOnce(&MerchantViewerDataManagerTest::LoadEntriesEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(),
                     hostnames_before_deletion));
  run_loop[2].Run();

  service_->DeleteMerchantViewerDataForTimeRange(start_time, end_time);
  task_environment_.RunUntilIdle();
  vector<string> hostnames_after_deletion = {};

  db->LoadAllEntries(
      base::BindOnce(&MerchantViewerDataManagerTest::LoadEntriesEvaluation,
                     base::Unretained(this), run_loop[3].QuitClosure(),
                     hostnames_after_deletion));
  run_loop[3].Run();

  histogram_tester.ExpectUniqueSample(
      "MerchantViewer.DataManager.ForceClearMerchantsForTimeRange", true, 1);
}
