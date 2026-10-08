// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/chrome_restart_request.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_store.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// The timeout of the local state commit in ChromeRestartRequest.
constexpr base::TimeDelta kCommitTimeout = base::Seconds(3);

// Holds the CommitPendingWrite() replies until CompletePendingCommits() is
// called, so that tests can decide when the commit completes.
class DeferredCommitPrefStore : public TestingPrefStore {
 public:
  DeferredCommitPrefStore() = default;

  // TestingPrefStore:
  void CommitPendingWrite(
      base::OnceClosure reply_callback,
      base::OnceClosure synchronous_done_callback) override {
    TestingPrefStore::CommitPendingWrite(base::OnceClosure(),
                                         std::move(synchronous_done_callback));
    pending_replies_.push_back(std::move(reply_callback));
  }

  // Runs the held CommitPendingWrite() replies.
  void CompletePendingCommits() {
    for (auto& reply : std::exchange(pending_replies_, {})) {
      if (reply) {
        std::move(reply).Run();
      }
    }
  }

 private:
  ~DeferredCommitPrefStore() override = default;

  std::vector<base::OnceClosure> pending_replies_;
};

class ChromeRestartRequestTest : public testing::Test {
 protected:
  void SetUp() override {
    FakeSessionManagerClient::Get()->set_supports_browser_restart(true);
    // On success, session manager kills Chrome before replying to RestartJob.
    FakeSessionManagerClient::Get()->set_defer_restart_job_reply(true);

    PrefServiceFactory factory;
    factory.set_user_prefs(pref_store_);
    local_state_ = factory.Create(base::MakeRefCounted<PrefRegistrySimple>());
  }

  void TearDown() override {
    // Reply as if session manager rejected the request, so that the request
    // deletes itself.
    FakeSessionManagerClient::Get()->SendDeferredRestartJobReplies(
        /*result=*/false);
    ResetRestartRequestedForTesting();
  }

  void StartRequest() {
    RestartChrome(*local_state_,
                  base::CommandLine(base::CommandLine::NO_PROGRAM),
                  RestartChromeReason::kGuest);
  }

  int GetRestartJobCallCount() const {
    return FakeSessionManagerClient::Get()->restart_job_call_count();
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ScopedFakeSessionManagerClient scoped_session_manager_client_{
      FakeSessionManagerClient::PolicyStorageType::kInMemory};
  // Backs `local_state_` and holds its commit replies.
  scoped_refptr<DeferredCommitPrefStore> pref_store_ =
      base::MakeRefCounted<DeferredCommitPrefStore>();
  // Local state passed to RestartChrome(). TestingBrowserProcess::local_state()
  // is not used because its commit always completes immediately, while the
  // tests need to control when the commit completes via `pref_store_`.
  std::unique_ptr<PrefService> local_state_;
};

TEST_F(ChromeRestartRequestTest, CommitCompletesBeforeTimeout) {
  StartRequest();
  EXPECT_EQ(0, GetRestartJobCallCount());

  pref_store_->CompletePendingCommits();
  EXPECT_EQ(1, GetRestartJobCallCount());

  // The timeout should not send the request again.
  task_environment_.FastForwardBy(kCommitTimeout);
  EXPECT_EQ(1, GetRestartJobCallCount());
}

TEST_F(ChromeRestartRequestTest, TimeoutBeforeCommitCompletes) {
  StartRequest();
  EXPECT_EQ(0, GetRestartJobCallCount());

  task_environment_.FastForwardBy(kCommitTimeout);
  EXPECT_EQ(1, GetRestartJobCallCount());

  // The commit completion should not send the request again.
  pref_store_->CompletePendingCommits();
  EXPECT_EQ(1, GetRestartJobCallCount());
}

}  // namespace
}  // namespace ash
