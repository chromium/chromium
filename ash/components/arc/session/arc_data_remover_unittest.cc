// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_data_remover.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_prefs.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/upstart/fake_upstart_client.h"
#include "components/account_id/account_id.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class TestUpstartClient : public ash::FakeUpstartClient {
 public:
  TestUpstartClient() = default;

  TestUpstartClient(const TestUpstartClient&) = delete;
  TestUpstartClient& operator=(const TestUpstartClient&) = delete;

  ~TestUpstartClient() override = default;

  void StartJob(const std::string& job,
                const std::vector<std::string>& upstart_env,
                chromeos::VoidDBusMethodCallback callback) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), arc_available_));
  }

  void set_arc_available(bool arc_available) { arc_available_ = arc_available; }

 private:
  bool arc_available_ = false;
};

class ArcDataRemoverTest : public testing::Test {
 public:
  ArcDataRemoverTest() = default;

  ArcDataRemoverTest(const ArcDataRemoverTest&) = delete;
  ArcDataRemoverTest& operator=(const ArcDataRemoverTest&) = delete;

  void SetUp() override {
    test_upstart_client_ = std::make_unique<TestUpstartClient>();
    prefs::RegisterProfilePrefs(prefs_.registry());
  }

  void TearDown() override { test_upstart_client_.reset(); }

  PrefService* prefs() { return &prefs_; }

  const cryptohome::Identification& cryptohome_id() const {
    return cryptohome_id_;
  }

  TestUpstartClient* upstart_client() {
    return static_cast<TestUpstartClient*>(ash::UpstartClient::Get());
  }

 private:
  TestingPrefServiceSimple prefs_;
  const cryptohome::Identification cryptohome_id_{EmptyAccountId()};
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestUpstartClient> test_upstart_client_;
};

TEST_F(ArcDataRemoverTest, NotScheduled) {
  ArcDataRemover data_remover(prefs(), cryptohome_id());

  base::RunLoop loop;
  data_remover.Run(base::BindOnce(
      [](base::RunLoop* loop, std::optional<bool> result) {
        EXPECT_EQ(result, std::nullopt);
        loop->Quit();
      },
      &loop));
  loop.Run();
}

TEST_F(ArcDataRemoverTest, Success) {
  base::HistogramTester histogram_tester;

  upstart_client()->set_arc_available(true);

  ArcDataRemover data_remover(prefs(), cryptohome_id());
  data_remover.Schedule();

  base::RunLoop loop;
  data_remover.Run(base::BindOnce(
      [](base::RunLoop* loop, std::optional<bool> result) {
        EXPECT_EQ(result, std::make_optional(true));
        loop->Quit();
      },
      &loop));
  loop.Run();

  histogram_tester.ExpectUniqueSample("Arc.DataRemoved.Success", true, 1);
}

TEST_F(ArcDataRemoverTest, Fail) {
  base::HistogramTester histogram_tester;

  ArcDataRemover data_remover(prefs(), cryptohome_id());
  data_remover.Schedule();

  base::RunLoop loop;
  data_remover.Run(base::BindOnce(
      [](base::RunLoop* loop, std::optional<bool> result) {
        EXPECT_EQ(result, std::make_optional(false));
        loop->Quit();
      },
      &loop));
  loop.Run();

  histogram_tester.ExpectUniqueSample("Arc.DataRemoved.Success", false, 1);
}

TEST_F(ArcDataRemoverTest, PrefPersistsAcrossInstances) {
  {
    ArcDataRemover data_remover(prefs(), cryptohome_id());
    data_remover.Schedule();
    EXPECT_TRUE(data_remover.IsScheduledForTesting());
  }

  {
    ArcDataRemover data_remover(prefs(), cryptohome_id());
    EXPECT_TRUE(data_remover.IsScheduledForTesting());
  }
}

}  // namespace
}  // namespace arc
