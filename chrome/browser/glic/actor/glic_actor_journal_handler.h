// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_GLIC_ACTOR_GLIC_ACTOR_JOURNAL_HANDLER_H_
#define CHROME_BROWSER_GLIC_ACTOR_GLIC_ACTOR_JOURNAL_HANDLER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/common/actor.mojom-forward.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

class Profile;

namespace actor {
class ActorKeyedService;
class AggregatedJournalFileSerializer;
class AggregatedJournalInMemorySerializer;
}  // namespace actor

namespace glic {
// Class that encapsulates interacting with the actor journal.
class GlicActorJournalHandler {
 public:
  explicit GlicActorJournalHandler(Profile* profile);
  ~GlicActorJournalHandler();

  void LogBeginAsyncEvent(uint64_t event_async_id,
                          int32_t task_id,
                          const std::string& event,
                          const std::string& details);
  void LogEndAsyncEvent(uint64_t event_async_id, const std::string& details);
  void LogInstantEvent(int32_t task_id,
                       const std::string& event,
                       const std::string& details);
  void Clear();
  void Snapshot(
      bool clear_journal,
      glic::mojom::WebClientHandler::JournalSnapshotCallback callback);
  std::vector<uint8_t> GetSnapshot(bool clear_journal);
  void Start(uint64_t max_bytes, bool capture_screenshots);
  void Stop();
  void RecordFeedback(bool positive, const std::string& reason);

 private:
  void SendResponseFeedback(const std::string& reason);

  void GetUniquePath(const base::FilePath& file_path,
                     base::OnceCallback<void(const base::FilePath&)> callback);

  void OnUniquePathReceived(const base::FilePath& unique_path);
  void FileInitDone(bool success);

  absl::flat_hash_map<
      uint64_t,
      std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry>>
      active_journal_events_;
  std::unique_ptr<actor::AggregatedJournalInMemorySerializer>
      journal_serializer_;
  std::unique_ptr<actor::AggregatedJournalFileSerializer>
      file_journal_serializer_;
  raw_ptr<actor::ActorKeyedService> actor_keyed_service_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_ACTOR_GLIC_ACTOR_JOURNAL_HANDLER_H_
