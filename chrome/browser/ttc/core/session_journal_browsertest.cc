// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_journal.h"

#include <map>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/app/public/tool_types.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "chrome/browser/ttc/core/test_utils.h"
#include "chrome/browser/ttc/core/ttc_core_browser_test_base.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/core/task_id.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ttc {

namespace {

using JournalEntries = std::vector<ActorJournalRecorder::Entry>;

// Tests that a session's journal records under the session's actor task,
// including after the task is replaced. The journal's mechanics are covered by
// SessionJournalTest.
class TtcSessionJournalBrowserTest : public TtcCoreBrowserTestBase {
 public:
  TtcSessionJournalBrowserTest() = default;
  ~TtcSessionJournalBrowserTest() override = default;

  // TtcCoreBrowserTestBase:
  void SetUpOnMainThread() override {
    TtcCoreBrowserTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_https_test_server().Start());

    actor_service_ = actor::ActorKeyedService::Get(profile());
    ASSERT_TRUE(actor_service_);
    recorder_.emplace(actor_service_->GetJournal());
  }

  void TearDownOnMainThread() override {
    recorder_.reset();
    actor_service_ = nullptr;
    TtcCoreBrowserTestBase::TearDownOnMainThread();
  }

 protected:
  actor::ActorKeyedService& actor_service() { return *actor_service_; }

  ActorJournalRecorder& recorder() { return *recorder_; }

  actor::TaskId GetOnlyActiveTaskId() {
    const std::map<actor::TaskId, const actor::ActorTask*> tasks =
        actor_service().GetActiveTasks();
    EXPECT_EQ(tasks.size(), 1u);
    return tasks.size() == 1u ? tasks.begin()->first : actor::TaskId();
  }

  ToolResponse OpenUrl(const GURL& url) {
    ToolRequest request;
    request.name = "open_url";
    request.arguments.Set("url", url.spec());
    request.arguments.Set("new_tab", false);
    base::test::TestFuture<ToolResponse> future;
    ttc_service().session_controller()->ProcessToolCall(request,
                                                        future.GetCallback());
    return future.Take();
  }

  // Expects `entry` to be on the front end track of the task with `task_id`.
  void ExpectOnTaskTrack(const ActorJournalRecorder::Entry& entry,
                         actor::TaskId task_id) {
    EXPECT_EQ(entry.task_id, task_id);
    EXPECT_EQ(entry.track_uuid, actor::MakeFrontEndTrackUUID(task_id));
  }

 private:
  raw_ptr<actor::ActorKeyedService> actor_service_ = nullptr;
  std::optional<ActorJournalRecorder> recorder_;
};

IN_PROC_BROWSER_TEST_F(TtcSessionJournalBrowserTest,
                       JournalFollowsTheSessionTask) {
  ttc_service().StartSession();
  SessionJournal& journal = ttc_service().session_controller()->GetJournal();
  const actor::TaskId first_task_id = GetOnlyActiveTaskId();
  ASSERT_FALSE(first_task_id.is_null());

  journal.Log("TestEvent", {});

  // Stopping the task outside of the session makes the next tool call replace
  // it. The session keeps its journal, which moves to the new task.
  actor_service().StopTask(first_task_id,
                           actor::ActorTask::StoppedReason::kStoppedByUser);
  ASSERT_TRUE(OpenUrl(embedded_https_test_server().GetURL("example.com",
                                                          "/title1.html"))
                  .Ok());
  const actor::TaskId second_task_id = GetOnlyActiveTaskId();
  ASSERT_FALSE(second_task_id.is_null());
  ASSERT_NE(second_task_id, first_task_id);
  ASSERT_EQ(&ttc_service().session_controller()->GetJournal(), &journal);

  journal.Log("TestEvent", {});

  const JournalEntries entries = recorder().GetEntries("TestEvent");
  ASSERT_EQ(entries.size(), 2u);
  ExpectOnTaskTrack(entries[0], first_task_id);
  ExpectOnTaskTrack(entries[1], second_task_id);

  // The tool call that needed the new task is journaled under it.
  const JournalEntries tool_calls = recorder().GetEntries("TtcToolCall");
  ASSERT_EQ(tool_calls.size(), 2u);
  ExpectOnTaskTrack(tool_calls[0], second_task_id);
  ExpectOnTaskTrack(tool_calls[1], second_task_id);
}

}  // namespace

}  // namespace ttc
