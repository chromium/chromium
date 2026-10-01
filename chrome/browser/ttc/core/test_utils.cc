// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/test_utils.h"

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"

namespace ttc {

MockConversation::MockConversation() = default;

MockConversation::~MockConversation() = default;

ActorJournalRecorder::Entry::Entry() = default;

ActorJournalRecorder::Entry::Entry(const Entry&) = default;

ActorJournalRecorder::Entry& ActorJournalRecorder::Entry::operator=(
    const Entry&) = default;

ActorJournalRecorder::Entry::~Entry() = default;

ActorJournalRecorder::ActorJournalRecorder(actor::AggregatedJournal& journal) {
  observation_.Observe(&journal);
}

ActorJournalRecorder::~ActorJournalRecorder() = default;

std::vector<ActorJournalRecorder::Entry> ActorJournalRecorder::GetEntries(
    std::string_view event) const {
  std::vector<Entry> entries;
  for (const Entry& entry : entries_) {
    if (entry.event == event) {
      entries.push_back(entry);
    }
  }
  return entries;
}

void ActorJournalRecorder::WillAddJournalEntry(
    const actor::AggregatedJournal::Entry& entry) {
  Entry& recorded = entries_.emplace_back();
  recorded.type = entry.data->type;
  recorded.event = entry.data->event;
  recorded.task_id = entry.data->task_id;
  recorded.track_uuid = entry.data->track_uuid;
  for (const actor::mojom::JournalDetailsPtr& detail : entry.data->details) {
    recorded.details.emplace(detail->key, detail->value);
  }
}

void TinyWait() {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();
}

}  // namespace ttc
