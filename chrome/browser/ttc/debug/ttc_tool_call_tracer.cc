// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/debug/ttc_tool_call_tracer.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/ttc/debug/ttc_debug_session.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/public/mojom/actor_types.mojom-shared.h"
#include "components/tabs/public/tab_interface.h"
#include "components/ttc/app/public/tool_types.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"

namespace ttc {

namespace {

char EntryTypeChar(actor::mojom::JournalEntryType type) {
  switch (type) {
    case actor::mojom::JournalEntryType::kBegin:
      return 'B';
    case actor::mojom::JournalEntryType::kEnd:
      return 'E';
    case actor::mojom::JournalEntryType::kInstant:
      return 'I';
  }
}

// Snapshot of `contents` for the debug record.
TtcDebugSession::TabSnapshot SnapshotOf(content::WebContents& contents) {
  TtcDebugSession::TabSnapshot snapshot;
  snapshot.url = contents.GetLastCommittedURL().spec();
  snapshot.title = base::UTF16ToUTF8(contents.GetTitle());
  snapshot.visible = contents.GetVisibility() == content::Visibility::VISIBLE;
  if (tabs::TabInterface* tab =
          tabs::TabInterface::MaybeGetFromContents(&contents)) {
    snapshot.tab_id = tab->GetHandle().raw_value();
    snapshot.active_in_window = tab->IsActivated();
  }
  return snapshot;
}

}  // namespace

TtcToolCallTracer::PendingTool::PendingTool() = default;
TtcToolCallTracer::PendingTool::PendingTool(PendingTool&&) = default;
TtcToolCallTracer::PendingTool& TtcToolCallTracer::PendingTool::operator=(
    PendingTool&&) = default;
TtcToolCallTracer::PendingTool::~PendingTool() = default;

TtcToolCallTracer::TtcToolCallTracer(TtcDebugSession& session,
                                     actor::AggregatedJournal& actor_journal,
                                     TaskIdGetter task_id_getter)
    : session_(session), task_id_getter_(std::move(task_id_getter)) {
  actor_journal_observation_.Observe(&actor_journal);
}

TtcToolCallTracer::~TtcToolCallTracer() = default;

void TtcToolCallTracer::OnToolDispatched(std::string_view call_id,
                                         std::string_view name,
                                         const base::DictValue& arguments) {
  PendingTool pending;
  pending.expectation =
      TtcToolEffectVerifier::ExpectationForTool(name, arguments);
  pending.expected_host = TtcToolEffectVerifier::ExpectedHost(name, arguments);
  pending_tools_.insert_or_assign(std::string(call_id), std::move(pending));
  session_->OnToolDispatched(call_id, name,
                             base::WriteJson(arguments).value_or(""));
}

void TtcToolCallTracer::OnToolTarget(std::string_view call_id,
                                     content::WebContents& target) {
  // ToolController reports the target of the tool call it's running, which
  // it reported dispatched first.
  auto it = pending_tools_.find(call_id);
  CHECK(it != pending_tools_.end());
  PendingTool& pending = it->second;
  pending.target = target.GetWeakPtr();
  if (pending.expectation) {
    pending.verifier = std::make_unique<TtcToolEffectVerifier>(
        &target, *pending.expectation, std::move(pending.expected_host),
        TtcToolEffectVerifier::kTimeout, base::DefaultTickClock::GetInstance(),
        base::BindOnce(&TtcToolCallTracer::OnToolEffect,
                       weak_factory_.GetWeakPtr(), std::string(call_id)));
  }
  session_->OnToolTarget(call_id, SnapshotOf(target),
                         /*verifying=*/!!pending.verifier);
}

void TtcToolCallTracer::OnToolFinished(std::string_view call_id,
                                       const ToolResponse& response) {
  const bool ok = response.Ok();
  // Only calls reported dispatched finish, and each only once.
  auto it = pending_tools_.find(call_id);
  CHECK(it != pending_tools_.end());
  std::optional<TtcDebugSession::TabSnapshot> after;
  if (it->second.target) {
    after = SnapshotOf(*it->second.target);
  }
  // A failed tool has no effect to verify; a successful one keeps its verifier
  // until it reports.
  if (!ok || !it->second.verifier) {
    pending_tools_.erase(it);
  } else {
    it->second.finished = true;
  }
  if (ok) {
    session_->OnToolSucceeded(call_id, after);
    return;
  }
  const ToolError& error = response.error();
  session_->OnToolFailed(call_id,
                         error.code ? TtcDebugSession::ActionResultCodeName(
                                          static_cast<int32_t>(*error.code))
                                    : "kError",
                         error.message.value_or(""), after);
}

void TtcToolCallTracer::OnToolEffect(std::string call_id,
                                     bool verified,
                                     std::string detail) {
  session_->OnToolEffect(call_id, verified, detail);
  // The verifier belongs to the pending tool, so the tool is still pending.
  // The effect often lands before the actor reports its result, so keep the
  // target until then for the after-snapshot.
  auto it = pending_tools_.find(call_id);
  CHECK(it != pending_tools_.end());
  it->second.verifier.reset();
  if (it->second.finished) {
    pending_tools_.erase(it);
  }
}

void TtcToolCallTracer::WillAddJournalEntry(
    const actor::AggregatedJournal::Entry& entry) {
  if (!entry.data) {
    return;
  }
  const actor::TaskId task_id = task_id_getter_.Run();
  if (task_id.is_null() || entry.data->task_id != task_id) {
    return;
  }
  // Only the actor framework's own tracks. What TTC logs on its tracks is
  // already recorded directly, whichever tracks those are.
  const uint64_t track = entry.data->track_uuid;
  if (track != actor::MakeBrowserTrackUUID(task_id) &&
      track != actor::MakeRendererTrackUUID(task_id)) {
    return;
  }
  std::string details;
  bool has_error = false;
  for (const auto& detail : entry.data->details) {
    if (!details.empty()) {
      details += "; ";
    }
    base::StrAppend(&details, {detail->key, "=", detail->value});
    has_error |= detail->key == "error";
  }
  // Handled asynchronously: this runs inside the journal's own logging.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TtcToolCallTracer::OnActorJournalEntry,
                                weak_factory_.GetWeakPtr(), entry.data->event,
                                EntryTypeChar(entry.data->type),
                                std::move(details), has_error));
}

void TtcToolCallTracer::OnActorJournalEntry(std::string event,
                                            char type,
                                            std::string details,
                                            bool has_error) {
  session_->OnActorJournalEntry(event, type, details, has_error);
}

}  // namespace ttc
