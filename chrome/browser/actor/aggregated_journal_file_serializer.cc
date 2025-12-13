// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/aggregated_journal_file_serializer.h"

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"

namespace actor {

// An object that is sequence bound to a background pool worker. This
// allows us to implement the blocking IO work on the background task.
class AggregatedJournalFileSerializer::FileWriter {
 public:
  FileWriter() = default;

  bool Init(const base::FilePath& file_path) {
    file_handle_ = std::make_unique<base::File>(
        file_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    if (!file_handle_->IsValid()) {
      return false;
    }
    return true;
  }

  void WritePacket(std::vector<uint8_t> message) {
    perfetto::TracePacket packet;
    packet.AddSlice(message.data(), message.size());

    auto [preamble, preamble_size] = packet.GetProtoPreamble();
    if (!file_handle_->WriteAtCurrentPosAndCheck(UNSAFE_TODO(base::span(
            reinterpret_cast<const uint8_t*>(preamble), preamble_size)))) {
      return;
    }

    for (const perfetto::Slice& slice : packet.slices()) {
      if (!file_handle_->WriteAtCurrentPosAndCheck(UNSAFE_TODO(base::span(
              static_cast<const uint8_t*>(slice.start), slice.size)))) {
        return;
      }
    }
  }

  void Shutdown() { file_handle_.reset(); }

 private:
  std::unique_ptr<base::File> file_handle_;
};

AggregatedJournalFileSerializer::AggregatedJournalFileSerializer(
    AggregatedJournal& journal)
    : AggregatedJournalSerializer(journal) {}

AggregatedJournalFileSerializer::~AggregatedJournalFileSerializer() {
  file_writer_.Reset();
}

void AggregatedJournalFileSerializer::Init(const base::FilePath& file_path,
                                           InitResult callback) {
  file_writer_ = base::SequenceBound<FileWriter>(
      base::ThreadPool::CreateSequencedTaskRunnerForResource(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
          file_path));
  file_writer_.AsyncCall(&FileWriter::Init)
      .WithArgs(file_path)
      .Then(base::BindOnce(&AggregatedJournalFileSerializer::InitDone,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(callback)));
}

void AggregatedJournalFileSerializer::InitDone(InitResult callback,
                                               bool success) {
  if (success) {
    InitImpl();
  }
  std::move(callback).Run(success);
}

void AggregatedJournalFileSerializer::Shutdown(base::OnceClosure callback) {
  file_writer_.AsyncCall(&FileWriter::Shutdown).Then(std::move(callback));
}

void AggregatedJournalFileSerializer::WriteTracePacket(
    std::vector<uint8_t> message) {
  file_writer_.AsyncCall(&FileWriter::WritePacket).WithArgs(std::move(message));
}

}  // namespace actor
