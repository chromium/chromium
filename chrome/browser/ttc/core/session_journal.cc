// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_journal.h"

#include <utility>

#include "base/check_op.h"
#include "chrome/browser/ttc/core/server_journal_event.h"
#include "components/actor/core/journal_details_builder.h"
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
  // TODO(bokan): Use server provided time.
  // TODO(bokan): Implement async events
  journal_->Log(GURL(), task_id_, actor::MakeTtcBackendTrackUUID(task_id_),
                event.name, std::move(details_builder).Build());
}

}  // namespace ttc
