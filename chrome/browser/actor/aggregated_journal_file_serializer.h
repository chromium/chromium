// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_AGGREGATED_JOURNAL_FILE_SERIALIZER_H_
#define CHROME_BROWSER_ACTOR_AGGREGATED_JOURNAL_FILE_SERIALIZER_H_

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "chrome/browser/actor/aggregated_journal_serializer.h"

namespace actor {

// A class that serializes to an AggregatedJournal to a file.
class AggregatedJournalFileSerializer : public AggregatedJournalSerializer {
 public:
  explicit AggregatedJournalFileSerializer(AggregatedJournal& journal);
  ~AggregatedJournalFileSerializer() override;

  using InitResult = base::OnceCallback<void(bool)>;
  void Init(const base::FilePath& file_path, InitResult callback);
  void Shutdown(base::OnceClosure callback);

 protected:
  void WriteTracePacket(std::vector<uint8_t> message) override;

 private:
  void InitDone(InitResult callback, bool success);

  class FileWriter;
  base::SequenceBound<FileWriter> file_writer_;

  base::WeakPtrFactory<AggregatedJournalFileSerializer> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_AGGREGATED_JOURNAL_FILE_SERIALIZER_H_
