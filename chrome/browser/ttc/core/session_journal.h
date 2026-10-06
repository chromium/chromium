// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_SESSION_JOURNAL_H_
#define CHROME_BROWSER_TTC_CORE_SESSION_JOURNAL_H_

#include <stdint.h>

#include <memory>
#include <string_view>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace ttc {

struct ServerJournalEvent;

// Records a TTC session's events in the actor journal, on the "Front End"
// track of the session's current actor task.
//
// TODO(b/555804825): Entries aren't associated with a page. Rather than tagging
// each entry with a URL, journal the active page's details on a track of their
// own, updated whenever the active page changes (e.g. from ActiveTabTracker).
class SessionJournal {
 public:
  // An async event that ends when EndEntry() is called on it or when it's
  // destroyed.
  using PendingAsyncEvent = actor::AggregatedJournal::PendingAsyncEntry;

  // `journal` must outlive `this`.
  SessionJournal(actor::AggregatedJournal& journal, actor::TaskId task_id);
  SessionJournal(const SessionJournal&) = delete;
  SessionJournal& operator=(const SessionJournal&) = delete;
  ~SessionJournal();

  // Attributes subsequent events to `new_task_id`, e.g. when the session's task
  // is replaced, and logs the change under both the previous and new tasks.
  // Pending async events stay on the track of the task they began under.
  // `new_task_id` must differ from the current task ID.
  //
  // A single session currently may require starting multiple actor tasks,
  // since the session's task can be stopped outside of TTC (e.g. when the user
  // closes a tab it acted on) and must then be replaced.
  // TODO(b/552544497): Remove this once a session's task can only be stopped
  // by the session itself.
  void SetTaskId(actor::TaskId new_task_id);

  // Logs an instant event.
  void Log(std::string_view event_name,
           std::vector<actor::mojom::JournalDetailsPtr> details);

  // Begins an async event, which lasts until the returned object ends it.
  [[nodiscard]] std::unique_ptr<PendingAsyncEvent> BeginAsyncEvent(
      std::string_view event_name,
      std::vector<actor::mojom::JournalDetailsPtr> details);

  // Records a journal event received from the server.
  void HandleServerJournalEvent(const ServerJournalEvent& event);

  actor::TaskId task_id() const { return task_id_; }
  base::TimeDelta server_clock_delta() const { return server_clock_delta_; }

 private:
  const raw_ref<actor::AggregatedJournal> journal_;
  actor::TaskId task_id_;
  bool received_clock_sync_ = false;
  base::TimeDelta server_clock_delta_;
  absl::flat_hash_map<int32_t, std::unique_ptr<PendingAsyncEvent>>
      pending_server_async_events_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_SESSION_JOURNAL_H_
