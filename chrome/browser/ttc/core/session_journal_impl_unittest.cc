// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_journal_impl.h"

#include <memory>
#include <vector>

#include "base/strings/to_string.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ttc/core/test_utils.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {

namespace {

using ::actor::mojom::JournalEntryType;
using ::testing::Contains;
using ::testing::Pair;

using JournalEntries = std::vector<ActorJournalRecorder::Entry>;

constexpr actor::TaskId kFirstTaskId(1);
constexpr actor::TaskId kSecondTaskId(2);

class SessionJournalImplTest : public testing::Test {
 protected:
  ActorJournalRecorder& recorder() { return recorder_; }

  // Expects `entry` to be on the front end track of the task with `task_id`.
  void ExpectOnTaskTrack(const ActorJournalRecorder::Entry& entry,
                         actor::TaskId task_id) {
    EXPECT_EQ(entry.task_id, task_id);
    EXPECT_EQ(entry.track_uuid, actor::MakeFrontEndTrackUUID(task_id));
  }

  base::test::TaskEnvironment task_environment_;
  actor::AggregatedJournal aggregated_journal_;
  ActorJournalRecorder recorder_{aggregated_journal_};
  SessionJournalImpl journal_{aggregated_journal_, kFirstTaskId};
};

TEST_F(SessionJournalImplTest, LogsOnTheTaskTrack) {
  journal_.Log("TestEvent",
               actor::JournalDetailsBuilder().Add("key", "value").Build());

  const JournalEntries entries = recorder().GetEntries("TestEvent");
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].type, JournalEntryType::kInstant);
  ExpectOnTaskTrack(entries[0], kFirstTaskId);
  EXPECT_THAT(entries[0].details, Contains(Pair("key", "value")));
}

TEST_F(SessionJournalImplTest, LogsOnMesClientTracks) {
  journal_.Log("RecvEvent",
               actor::JournalDetailsBuilder().Add("dir", "recv").Build(),
               SessionJournal::Track::kMesClientRecv);
  journal_.Log("SendEvent",
               actor::JournalDetailsBuilder().Add("dir", "send").Build(),
               SessionJournal::Track::kMesClientSend);

  const JournalEntries recv_entries = recorder().GetEntries("RecvEvent");
  ASSERT_EQ(recv_entries.size(), 1u);
  EXPECT_EQ(recv_entries[0].type, JournalEntryType::kInstant);
  EXPECT_EQ(recv_entries[0].task_id, kFirstTaskId);
  EXPECT_EQ(recv_entries[0].track_uuid,
            actor::MakeTtcMesClientRecvTrackUUID(kFirstTaskId));
  EXPECT_THAT(recv_entries[0].details, Contains(Pair("dir", "recv")));

  const JournalEntries send_entries = recorder().GetEntries("SendEvent");
  ASSERT_EQ(send_entries.size(), 1u);
  EXPECT_EQ(send_entries[0].type, JournalEntryType::kInstant);
  EXPECT_EQ(send_entries[0].task_id, kFirstTaskId);
  EXPECT_EQ(send_entries[0].track_uuid,
            actor::MakeTtcMesClientSendTrackUUID(kFirstTaskId));
  EXPECT_THAT(send_entries[0].details, Contains(Pair("dir", "send")));
}

TEST_F(SessionJournalImplTest, SetTaskIdLogsTheChangeUnderBothTasks) {
  journal_.SetTaskId(kSecondTaskId);

  EXPECT_EQ(journal_.task_id(), kSecondTaskId);
  const JournalEntries changes = recorder().GetEntries("TtcSessionTaskChanged");
  ASSERT_EQ(changes.size(), 2u);
  ExpectOnTaskTrack(changes[0], kFirstTaskId);
  ExpectOnTaskTrack(changes[1], kSecondTaskId);
  for (const ActorJournalRecorder::Entry& change : changes) {
    EXPECT_THAT(change.details, Contains(Pair("previous_task_id",
                                              base::ToString(kFirstTaskId))));
    EXPECT_THAT(change.details,
                Contains(Pair("new_task_id", base::ToString(kSecondTaskId))));
  }
}

TEST_F(SessionJournalImplTest, SetTaskIdToTheCurrentTaskCrashes) {
  EXPECT_CHECK_DEATH(journal_.SetTaskId(kFirstTaskId));
}

TEST_F(SessionJournalImplTest, LogsOnTheNewTaskTrackAfterSetTaskId) {
  journal_.SetTaskId(kSecondTaskId);
  journal_.Log("TestEvent", {});

  const JournalEntries entries = recorder().GetEntries("TestEvent");
  ASSERT_EQ(entries.size(), 1u);
  ExpectOnTaskTrack(entries[0], kSecondTaskId);
}

TEST_F(SessionJournalImplTest, AsyncEventEndsWithDetails) {
  std::unique_ptr<SessionJournalImpl::PendingAsyncEvent> event =
      journal_.BeginAsyncEvent("TestAsyncEvent", {});
  event->EndEntry(actor::JournalDetailsBuilder().Add("result", "ok").Build());

  const JournalEntries entries = recorder().GetEntries("TestAsyncEvent");
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries[0].type, JournalEntryType::kBegin);
  EXPECT_EQ(entries[1].type, JournalEntryType::kEnd);
  EXPECT_THAT(entries[1].details, Contains(Pair("result", "ok")));
  ExpectOnTaskTrack(entries[0], kFirstTaskId);
  ExpectOnTaskTrack(entries[1], kFirstTaskId);
}

TEST_F(SessionJournalImplTest, AsyncEventEndsWhenDestroyed) {
  std::unique_ptr<SessionJournalImpl::PendingAsyncEvent> event =
      journal_.BeginAsyncEvent("TestAsyncEvent", {});
  ASSERT_EQ(recorder().GetEntries("TestAsyncEvent").size(), 1u);

  event.reset();

  const JournalEntries entries = recorder().GetEntries("TestAsyncEvent");
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries[1].type, JournalEntryType::kEnd);
}

// An async event that spans a task change stays on the track it began on, so
// that its begin and end entries match up.
TEST_F(SessionJournalImplTest, AsyncEventStaysOnItsTaskTrackAcrossSetTaskId) {
  std::unique_ptr<SessionJournalImpl::PendingAsyncEvent> event =
      journal_.BeginAsyncEvent("TestAsyncEvent", {});
  journal_.SetTaskId(kSecondTaskId);
  event->EndEntry({});

  const JournalEntries entries = recorder().GetEntries("TestAsyncEvent");
  ASSERT_EQ(entries.size(), 2u);
  ExpectOnTaskTrack(entries[0], kFirstTaskId);
  ExpectOnTaskTrack(entries[1], kFirstTaskId);
}

}  // namespace

}  // namespace ttc
