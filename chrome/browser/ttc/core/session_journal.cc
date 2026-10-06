// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_journal.h"

#include <utility>

#include "base/check_op.h"
#include "base/time/time.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/ttc/app/public/server_journal_event.h"
#include "url/gurl.h"

namespace ttc {

SessionJournal::SessionJournal(actor::AggregatedJournal& journal,
                               actor::TaskId task_id)
    : journal_(journal), task_id_(task_id) {}

SessionJournal::~SessionJournal() = default;

void SessionJournal::SetTaskId(actor::TaskId new_task_id) {
  CHECK_NE(new_task_id, task_id_);

  const actor::TaskId previous_task_id = task_id_;
  task_id_ = new_task_id;
  for (actor::TaskId logged_task_id : {previous_task_id, new_task_id}) {
    journal_->Log(GURL(), logged_task_id,
                  actor::MakeFrontEndTrackUUID(logged_task_id),
                  "TtcSessionTaskChanged",
                  actor::JournalDetailsBuilder()
                      .Add("previous_task_id", previous_task_id)
                      .Add("new_task_id", new_task_id)
                      .Build());
  }
}

void SessionJournal::Log(std::string_view event_name,
                         std::vector<actor::mojom::JournalDetailsPtr> details) {
  journal_->Log(GURL(), task_id_, actor::MakeFrontEndTrackUUID(task_id_),
                event_name, std::move(details));
}

std::unique_ptr<SessionJournal::PendingAsyncEvent>
SessionJournal::BeginAsyncEvent(
    std::string_view event_name,
    std::vector<actor::mojom::JournalDetailsPtr> details) {
  return journal_->CreatePendingAsyncEntry(
      GURL(), task_id_, actor::MakeFrontEndTrackUUID(task_id_), event_name,
      std::move(details));
}

void SessionJournal::HandleServerJournalEvent(const ServerJournalEvent& event) {
  actor::JournalDetailsBuilder details_builder;
  for (const ServerJournalEvent::Details& detail : event.details) {
    details_builder.Add(detail.key, detail.value);
  }
  const base::Time timestamp = event.timestamp + server_clock_delta_;

  if (event.type != ServerJournalEvent::Type::kClockSync &&
      !received_clock_sync_) {
    // The ClockSync event must be the first journal event received.
    journal_->Log(GURL(), task_id_, actor::MakeFrontEndTrackUUID(task_id_),
                  "ServerJournalEventBeforeClockSync", {});
  }

  switch (event.type) {
    case ServerJournalEvent::Type::kInstant:
      journal_->Log(GURL(), task_id_, actor::MakeTtcBackendTrackUUID(task_id_),
                    timestamp, event.name, std::move(details_builder).Build());
      break;
    case ServerJournalEvent::Type::kAsyncBegin:
      // If there is a matching ID, make sure it terminates before the new
      // event is created.
      pending_server_async_events_.erase(event.async_event_id);
      pending_server_async_events_[event.async_event_id] =
          journal_->CreatePendingAsyncEntry(
              GURL(), task_id_, actor::MakeTtcBackendTrackUUID(task_id_),
              timestamp, event.name, std::move(details_builder).Build());
      break;
    case ServerJournalEvent::Type::kAsyncEnd: {
      auto it = pending_server_async_events_.find(event.async_event_id);
      if (it == pending_server_async_events_.end()) {
        details_builder.Add("event_name", event.name)
            .Add("async_event_id", event.async_event_id);
        journal_->Log(GURL(), task_id_,
                      actor::MakeTtcBackendTrackUUID(task_id_), timestamp,
                      "UnmatchedAsyncEnd", std::move(details_builder).Build());
        break;
      }
      it->second->EndEntry(timestamp, std::move(details_builder).Build());
      pending_server_async_events_.erase(it);
      break;
    }
    case ServerJournalEvent::Type::kClockSync: {
      received_clock_sync_ = true;
      if (event.sync_timestamp <= base::Time::UnixEpoch()) {
        // Avoid adjusting if the server doesn't provide a timestamp or sends
        // an obviously bogus one.
        journal_->Log(GURL(), task_id_, actor::MakeFrontEndTrackUUID(task_id_),
                      "Bad ClockSync Message",
                      actor::JournalDetailsBuilder()
                          .Add("sync_timestamp", event.sync_timestamp)
                          .Build());
        break;
      }
      const base::Time client_receive_time = base::Time::Now();
      const base::Time client_midpoint =
          event.sync_timestamp +
          (client_receive_time - event.sync_timestamp) / 2;
      server_clock_delta_ = client_midpoint - event.timestamp;
      details_builder.Add("clock_delta", server_clock_delta_);
      journal_->Log(GURL(), task_id_, actor::MakeTtcBackendTrackUUID(task_id_),
                    client_midpoint,
                    event.name.empty() ? "ClockSync" : event.name,
                    std::move(details_builder).Build());
      break;
    }
    case ServerJournalEvent::Type::kUnspecified:
      break;
  }
}

}  // namespace ttc
