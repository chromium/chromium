// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_AGGREGATED_JOURNAL_H_
#define CHROME_BROWSER_ACTOR_AGGREGATED_JOURNAL_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/ring_buffer.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/supports_user_data.h"
#include "base/types/pass_key.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/common/actor.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "url/gurl.h"

namespace actor {

// A class that amalgamates all the journal entries from various RenderFrames.
class AggregatedJournal {
 public:
  AggregatedJournal();
  ~AggregatedJournal();

  // Journal entry
  struct Entry {
    std::string url;
    std::optional<std::vector<uint8_t>> jpg_screenshot;
    mojom::JournalEntryPtr data;

    Entry(const std::string& location, mojom::JournalEntryPtr data);
    ~Entry();
  };

  // A pending async journal entry.
  class PendingAsyncEntry {
   public:
    // Creation of the event is only from the AggregatedJournal itself. Use
    // `AggregatedJournal::CreatePendingAsyncEntry` to create this object.
    PendingAsyncEntry(base::PassKey<AggregatedJournal>,
                      base::SafeRef<AggregatedJournal> journal,
                      TaskId task_id,
                      uint64_t trace_id,
                      std::string_view event_name);
    ~PendingAsyncEntry();

    // End an pending entry with additional details. This can only be called
    // once and will be automatically called from the destructor if it hasn't
    // been called.
    void EndEntry(std::string_view details);

    AggregatedJournal& GetJournal();
    TaskId GetTaskId();

   private:
    base::PassKey<AggregatedJournal> pass_key_;
    bool terminated_ = false;
    base::SafeRef<AggregatedJournal> journal_;
    TaskId task_id_;
    uint64_t trace_id_;
    std::string event_name_;
  };

  // Observing class for new entries.
  class Observer : public base::CheckedObserver {
   public:
    virtual void WillAddJournalEntry(const Entry& entry) = 0;
  };

  using EntryBuffer = base::RingBuffer<std::unique_ptr<Entry>, 20>;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Create an async entry. This will log a Begin Entry event and when the
  // PendingAsyncEntry object is destroyed the End Entry will be logged.
  std::unique_ptr<PendingAsyncEntry> CreatePendingAsyncEntry(
      const GURL& url,
      TaskId task_id,
      std::string_view event_name,
      std::string_view details);

  // Log an instant event.
  void Log(const GURL& url,
           TaskId task_id,
           std::string_view event_name,
           std::string_view details);

  // Screenshots need to be an instant event with a custom event name to be
  // decoded in perfetto.
  void LogScreenshot(const GURL& url,
                     TaskId task_id,
                     std::string_view mime_type,
                     const std::vector<uint8_t>& data);

  void EnsureJournalBound(content::RenderFrameHost& rfh);
  void AppendJournalEntries(content::RenderFrameHost* rfh,
                            std::vector<mojom::JournalEntryPtr> entries);
  EntryBuffer::Iterator Items() { return entries_.Begin(); }
  base::SafeRef<AggregatedJournal> GetSafeRef();
  void AddEndEvent(base::PassKey<AggregatedJournal>,
                   TaskId task_id,
                   uint64_t trace_id,
                   const std::string& event_name,
                   std::string_view details);

 private:
  void AddEntry(std::unique_ptr<Entry>);

  uint64_t next_trace_id_;
  base::ObserverList<Observer> observers_;
  EntryBuffer entries_;
  base::WeakPtrFactory<AggregatedJournal> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_AGGREGATED_JOURNAL_H_
