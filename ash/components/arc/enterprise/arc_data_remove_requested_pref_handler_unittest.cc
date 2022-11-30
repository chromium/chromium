// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/enterprise/arc_data_remove_requested_pref_handler.h"
#include "ash/components/arc/session/arc_data_remover.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class PrefService;

namespace arc {
namespace data_snapshotd {

class ArcDataRemoveRequestedPrefHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    arc::prefs::RegisterProfilePrefs(pref_service_.registry());
    arc_data_remover_ = std::make_unique<ArcDataRemover>(
        pref_service(), cryptohome::Identification());
  }

  void TearDown() override { arc_data_remover_.reset(); }

  PrefService* pref_service() { return &pref_service_; }
  ArcDataRemover* arc_data_remover() { return arc_data_remover_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<ArcDataRemover> arc_data_remover_;
};

TEST_F(ArcDataRemoveRequestedPrefHandlerTest, RemoveInProcess) {
  // Schedule ARC data remove before initializing observer.
  arc_data_remover()->Schedule();
  EXPECT_TRUE(arc_data_remover()->IsScheduledForTesting());
  base::RunLoop run_loop;
  auto handler = ArcDataRemoveRequestedPrefHandler::Create(
      pref_service(), run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(ArcDataRemoveRequestedPrefHandlerTest, BasicRemove) {
  EXPECT_FALSE(arc_data_remover()->IsScheduledForTesting());
  base::RunLoop run_loop;
  auto handler = ArcDataRemoveRequestedPrefHandler::Create(
      pref_service(), run_loop.QuitClosure());

  // Schedule ARC data remove.
  arc_data_remover()->Schedule();
  EXPECT_TRUE(arc_data_remover()->IsScheduledForTesting());
  run_loop.Run();
}

}  // namespace data_snapshotd
}  // namespace arc
