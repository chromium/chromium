// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_DEBUG_TTC_TOOL_CALL_TRACER_H_
#define CHROME_BROWSER_TTC_DEBUG_TTC_TOOL_CALL_TRACER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/ttc/debug/ttc_tool_effect_verifier.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/task_id.h"

namespace content {
class WebContents;
}

namespace ttc {

class ToolResponse;
class TtcDebugSession;

// Traces the tool calls of one TTC session through the browser, into the
// session's TtcDebugSession: when each starts running, which tab it acts on,
// how that tab looked before and after, whether the tool had its expected
// effect on it (TtcToolEffectVerifier), and what the actor framework logged
// for it.
//
// Lives exactly as long as the TTC session, while the TtcDebugSession it
// writes to is kept after the session for review.
class TtcToolCallTracer : public actor::AggregatedJournal::Observer {
 public:
  using TaskIdGetter = base::RepeatingCallback<actor::TaskId()>;

  // `session` and `actor_journal` must outlive this object. `task_id_getter`
  // returns the session's current actor task, and is only run while this
  // object exists.
  TtcToolCallTracer(TtcDebugSession& session,
                    actor::AggregatedJournal& actor_journal,
                    TaskIdGetter task_id_getter);
  TtcToolCallTracer(const TtcToolCallTracer&) = delete;
  TtcToolCallTracer& operator=(const TtcToolCallTracer&) = delete;
  ~TtcToolCallTracer() override;

  // The tool call started running.
  void OnToolDispatched(std::string_view call_id,
                        std::string_view name,
                        const base::DictValue& arguments);
  // The tool acts on `target`, the voice-focused tab. Only reported by tools
  // that act on a tab: browser-level tools such as switch_tab have no target.
  // Must follow OnToolDispatched() for `call_id`.
  void OnToolTarget(std::string_view call_id, content::WebContents& target);
  // The tool call finished with `response`. Must follow OnToolDispatched() for
  // `call_id`, once.
  void OnToolFinished(std::string_view call_id, const ToolResponse& response);

  // actor::AggregatedJournal::Observer:
  void WillAddJournalEntry(
      const actor::AggregatedJournal::Entry& entry) override;

 private:
  struct PendingTool {
    PendingTool();
    PendingTool(PendingTool&&);
    PendingTool& operator=(PendingTool&&);
    ~PendingTool();

    // What to check on the target once the tool reports it, if anything.
    std::optional<TtcToolEffectVerifier::Expectation> expectation;
    std::optional<std::string> expected_host;
    // Null until the tool reports its target, and for browser-level tools.
    base::WeakPtr<content::WebContents> target;
    // Null once the effect is reported, or if it isn't checked.
    std::unique_ptr<TtcToolEffectVerifier> verifier;
    // Whether the actor reported the result. The entry is dropped once both
    // the result and the effect are in.
    bool finished = false;
  };

  void OnToolEffect(std::string call_id, bool verified, std::string detail);
  void OnActorJournalEntry(std::string event,
                           char type,
                           std::string details,
                           bool has_error);

  const raw_ref<TtcDebugSession> session_;
  TaskIdGetter task_id_getter_;
  std::map<std::string, PendingTool, std::less<>> pending_tools_;
  base::ScopedObservation<actor::AggregatedJournal,
                          actor::AggregatedJournal::Observer>
      actor_journal_observation_{this};
  base::WeakPtrFactory<TtcToolCallTracer> weak_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_DEBUG_TTC_TOOL_CALL_TRACER_H_
