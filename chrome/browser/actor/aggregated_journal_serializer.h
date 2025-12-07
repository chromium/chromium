// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_AGGREGATED_JOURNAL_SERIALIZER_H_
#define CHROME_BROWSER_ACTOR_AGGREGATED_JOURNAL_SERIALIZER_H_

#include <set>

#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/common/actor/task_id.h"

namespace actor {

// A class that serializes the journal to perfetto protobuffers. This
// is an abstract class that will defer the output to subclasses.
class AggregatedJournalSerializer : public AggregatedJournal::Observer {
 public:
  explicit AggregatedJournalSerializer(AggregatedJournal& journal);
  ~AggregatedJournalSerializer() override;

  // AggregatedJournal::Observer implementation.
  void WillAddJournalEntry(const AggregatedJournal::Entry& entry) override;

 protected:
  // The subclass should call this when they are ready to accept data.
  void InitImpl();
  void WriteTracePreamble();
  void ObservedTaskId(TaskId task_id);
  void ObservedTrackId(uint64_t track_uuid,
                       TaskId task_id,
                       const std::string& event_name);

  // Subclasses should implement this to receive generated data.
  virtual void WriteTracePacket(std::vector<uint8_t> message) = 0;

 private:
  std::set<TaskId> observed_task_ids_;
  std::set<uint64_t> observed_track_ids_;
  base::SafeRef<AggregatedJournal> journal_;
  size_t sequence_id_ = 1;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_AGGREGATED_JOURNAL_SERIALIZER_H_
