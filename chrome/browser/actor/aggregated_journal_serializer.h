// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_AGGREGATED_JOURNAL_SERIALIZER_H_
#define CHROME_BROWSER_ACTOR_AGGREGATED_JOURNAL_SERIALIZER_H_

#include <set>

#include "chrome/browser/actor/aggregated_journal.h"

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
  void ObservedTaskId(int32_t task_id);

  // Subclasses should implement this to receive generated data.
  virtual void WriteTracePacket(std::vector<uint8_t> message) = 0;

 private:
  std::set<int32_t> observed_task_ids_;
  base::SafeRef<AggregatedJournal> journal_;
  size_t sequence_id_ = 1;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_AGGREGATED_JOURNAL_SERIALIZER_H_
