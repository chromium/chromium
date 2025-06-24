// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_AGGREGATED_JOURNAL_IN_MEMORY_SERIALIZER_H_
#define CHROME_BROWSER_ACTOR_AGGREGATED_JOURNAL_IN_MEMORY_SERIALIZER_H_

#include "base/files/file.h"
#include "chrome/browser/actor/aggregated_journal_serializer.h"

namespace actor {

// A class that serializes to an AggregatedJournal to an in-memory buffer.
class AggregatedJournalInMemorySerializer : public AggregatedJournalSerializer {
 public:
  explicit AggregatedJournalInMemorySerializer(AggregatedJournal& journal);
  ~AggregatedJournalInMemorySerializer() override;

  void Init();
  size_t ApproximateSnapshotSize();
  std::vector<uint8_t> Snapshot(size_t max_bytes);
  void Clear();

 protected:
  void WriteTracePacket(std::vector<uint8_t> message) override;

 private:
  std::vector<std::vector<uint8_t>> buffer_list_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_AGGREGATED_JOURNAL_IN_MEMORY_SERIALIZER_H_
