// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tool call tracing and real-time problem detection for TtcDebugSession.

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ttc/debug/ttc_debug_session.h"

namespace ttc {

namespace {

std::string Ms(double ms) {
  return base::StringPrintf("%.0f ms", ms);
}

std::string DescribeTab(const TtcDebugSession::TabSnapshot& tab) {
  return base::StringPrintf("tab %d \"%s\" %s (%s, %s)", tab.tab_id,
                            tab.title.c_str(), tab.url.c_str(),
                            tab.visible ? "visible" : "not visible",
                            tab.active_in_window ? "active in its window"
                                                 : "not active in its window");
}

bool IsInForeground(const TtcDebugSession::TabSnapshot& tab) {
  return tab.visible && tab.active_in_window;
}

// The kind of the "<kind>:<tool call id>" issue raised for a background target.
constexpr std::string_view kBackgroundTarget = "tool_background_target";

}  // namespace

TtcDebugSession::ToolCall::ToolCall() = default;
TtcDebugSession::ToolCall::ToolCall(const ToolCall&) = default;
TtcDebugSession::ToolCall& TtcDebugSession::ToolCall::operator=(
    const ToolCall&) = default;
TtcDebugSession::ToolCall::~ToolCall() = default;

TtcDebugSession::Issue::Issue() = default;
TtcDebugSession::Issue::Issue(const Issue&) = default;
TtcDebugSession::Issue& TtcDebugSession::Issue::operator=(const Issue&) =
    default;
TtcDebugSession::Issue::~Issue() = default;

// static
std::string_view TtcDebugSession::ToolStatusName(ToolStatus status) {
  switch (status) {
    case ToolStatus::kPending:
      return "pending";
    case ToolStatus::kRunning:
      return "running";
    case ToolStatus::kOk:
      return "ok";
    case ToolStatus::kError:
      return "error";
    case ToolStatus::kRejected:
      return "rejected";
  }
}

// static
std::string_view TtcDebugSession::ToolEffectName(ToolEffect effect) {
  switch (effect) {
    case ToolEffect::kNotChecked:
      return "not_checked";
    case ToolEffect::kChecking:
      return "checking";
    case ToolEffect::kVerified:
      return "verified";
    case ToolEffect::kNoEffect:
      return "no_effect";
  }
}

// static
std::string_view TtcDebugSession::IssueSeverityName(IssueSeverity severity) {
  switch (severity) {
    case IssueSeverity::kInfo:
      return "info";
    case IssueSeverity::kWarning:
      return "warning";
    case IssueSeverity::kError:
      return "error";
  }
}

// static
std::string_view TtcDebugSession::IssueCategoryName(IssueCategory category) {
  switch (category) {
    case IssueCategory::kTool:
      return "tool";
    case IssueCategory::kServer:
      return "server";
    case IssueCategory::kTransport:
      return "transport";
    case IssueCategory::kAudio:
      return "audio";
    case IssueCategory::kPageContext:
      return "page_context";
    case IssueCategory::kModel:
      return "model";
    case IssueCategory::kActor:
      return "actor";
  }
}

// ---------------------------------------------------------------------------
// Issues

// static
std::string TtcDebugSession::ToolIssueKey(std::string_view kind,
                                          int tool_call_id) {
  return base::StrCat({kind, ":", base::NumberToString(tool_call_id)});
}

TtcDebugSession::Issue* TtcDebugSession::FindOpenIssue(std::string_view key) {
  for (Issue& issue : issues_) {
    if (!issue.resolved && issue.key == key) {
      return &issue;
    }
  }
  return nullptr;
}

void TtcDebugSession::RaiseIssue(std::string key, IssueSpec spec) {
  if (ended_) {
    return;
  }
  TruncateInPlace(key, kMaxIssueKeyLength);
  TruncateInPlace(spec.title);
  TruncateInPlace(spec.detail);
  for (auto& entry : spec.evidence) {
    TruncateInPlace(entry.second);
  }
  const double now = NowMs();
  if (Issue* open = FindOpenIssue(key)) {
    ++open->count;
    open->last_ms = now;
    open->detail = std::move(spec.detail);
    open->evidence = std::move(spec.evidence);
    MarkIssueChanged(*open);
    return;
  }
  if (issues_.size() >= kMaxIssues) {
    // Count each dropped problem once, however often it recurs.
    if (!dropped_issue_keys_.contains(key)) {
      if (dropped_issue_keys_.size() < kMaxIssues) {
        dropped_issue_keys_.insert(key);
      }
      DropItem("issues");
    }
    return;
  }
  Issue issue;
  issue.id = static_cast<int>(issues_.size()) + 1;
  issue.key = std::move(key);
  issue.severity = spec.severity;
  issue.category = spec.category;
  issue.title = std::move(spec.title);
  issue.detail = std::move(spec.detail);
  issue.evidence = std::move(spec.evidence);
  issue.first_ms = now;
  issue.last_ms = now;
  issue.tool_call_id = spec.tool_call_id;
  if (!turns_.empty()) {
    issue.turn_index = turns_.back().index;
  }
  issues_.push_back(std::move(issue));
  const Issue& added = issues_.back();
  AddEvent(
      EventKind::kIssue, 0,
      base::StrCat({IssueSeverityName(added.severity), ": ", added.title}));
  LogToJournal("TtcDebugIssue",
               base::StrCat({IssueSeverityName(added.severity), ": ",
                             added.title, ". ", added.detail}));
  MarkIssueChanged(added);
}

void TtcDebugSession::ResolveIssue(std::string_view key) {
  Issue* open = FindOpenIssue(key);
  if (!open || ended_) {
    return;
  }
  open->resolved = true;
  open->resolved_ms = NowMs();
  MarkIssueChanged(*open);
}

void TtcDebugSession::SetCondition(std::string_view key,
                                   bool active,
                                   base::FunctionRef<IssueSpec()> spec) {
  if (!active) {
    ResolveIssue(key);
  } else if (!FindOpenIssue(key)) {
    RaiseIssue(std::string(key), spec());
  }
}

void TtcDebugSession::MarkIssueChanged(const Issue& issue) {
  // Observers fetch the full state when they start observing.
  if (observers_.empty()) {
    return;
  }
  changed_issues_.insert(issue.id);
  MaybeStartFlushTimer();
}

// ---------------------------------------------------------------------------
// Tool calls

// static
TtcDebugSession::TabSnapshot TtcDebugSession::TruncateTab(
    const TabSnapshot& tab) {
  TabSnapshot truncated = tab;
  TruncateInPlace(truncated.url);
  TruncateInPlace(truncated.title);
  return truncated;
}

TtcDebugSession::ToolCall* TtcDebugSession::FindToolCall(
    std::string_view call_id) {
  // Stored ids are truncated.
  std::string truncated;
  if (call_id.size() > kMaxTextLength) {
    truncated = Truncate(call_id);
    call_id = truncated;
  }
  // The most recent call with this id; ids are unique per session in practice.
  for (auto it = tool_calls_.rbegin(); it != tool_calls_.rend(); ++it) {
    if (it->call_id == call_id) {
      return &*it;
    }
  }
  return nullptr;
}

TtcDebugSession::ToolCall* TtcDebugSession::FindOrCreateToolCall(
    std::string_view call_id,
    std::string_view name) {
  if (ToolCall* existing = FindToolCall(call_id)) {
    return existing;
  }
  if (tool_calls_.size() >= kMaxToolCalls) {
    // A call is usually reported several times (received, dispatched,
    // finished, response sent); count it once.
    std::string truncated_id = Truncate(call_id);
    if (truncated_id != last_dropped_call_id_) {
      last_dropped_call_id_ = std::move(truncated_id);
      DropItem("tool calls");
    }
    return nullptr;
  }
  const double now = NowMs();
  ToolCall tool_call;
  tool_call.id = static_cast<int>(tool_calls_.size()) + 1;
  tool_call.call_id = Truncate(call_id);
  tool_call.name = Truncate(name);
  tool_call.received_ms = now;
  if (Turn* turn = EnsureOpenTurn(last_speech_end_ms_.value_or(now))) {
    tool_call.turn_index = turn->index;
  }
  tool_calls_.push_back(std::move(tool_call));
  MarkToolCallChanged(tool_calls_.back());
  return &tool_calls_.back();
}

void TtcDebugSession::MarkToolCallChanged(const ToolCall& tool_call) {
  // Observers fetch the full state when they start observing.
  if (observers_.empty()) {
    return;
  }
  changed_tool_calls_.insert(tool_call.id);
  MaybeStartFlushTimer();
}

TtcDebugSession::ToolCall* TtcDebugSession::RunningToolCall() {
  for (auto it = tool_calls_.rbegin(); it != tool_calls_.rend(); ++it) {
    if (it->status == ToolStatus::kRunning) {
      return &*it;
    }
  }
  return nullptr;
}

TtcDebugSession::Evidence TtcDebugSession::ToolEvidence(
    const ToolCall& tool_call) const {
  Evidence evidence = {
      {"tool", tool_call.name},
      {"tool_call", base::StrCat({"#", base::NumberToString(tool_call.id),
                                  " (call_id ", tool_call.call_id, ")"})},
      {"arguments", tool_call.arguments_json},
  };
  if (tool_call.target_before) {
    evidence.emplace_back("target_at_dispatch",
                          DescribeTab(*tool_call.target_before));
  }
  if (tool_call.target_after) {
    evidence.emplace_back("target_at_finish",
                          DescribeTab(*tool_call.target_after));
  }
  if (!tool_call.result_code.empty()) {
    evidence.emplace_back("actor_result", tool_call.result_code);
  }
  if (!tool_call.result_message.empty()) {
    evidence.emplace_back("actor_message", tool_call.result_message);
  }
  if (!tool_call.effect_detail.empty()) {
    evidence.emplace_back("effect", tool_call.effect_detail);
  }
  if (!tool_call.actor_steps.empty()) {
    evidence.emplace_back("last_actor_step", tool_call.actor_steps.back());
  }
  return evidence;
}

void TtcDebugSession::RaiseToolIssue(std::string_view kind,
                                     const ToolCall& tool_call,
                                     IssueSeverity severity,
                                     std::string title,
                                     std::string detail) {
  RaiseIssue(ToolIssueKey(kind, tool_call.id),
             {.severity = severity,
              .category = IssueCategory::kTool,
              .title = std::move(title),
              .detail = std::move(detail),
              .evidence = ToolEvidence(tool_call),
              .tool_call_id = tool_call.id});
}

void TtcDebugSession::OnToolCallRejected(std::string_view call_id,
                                         std::string_view reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ended_) {
    return;
  }
  ToolCall* tool_call = FindOrCreateToolCall(call_id, "");
  if (!tool_call) {
    return;
  }
  tool_call->status = ToolStatus::kRejected;
  tool_call->finished_ms = NowMs();
  tool_call->result_message = Truncate(reason);
  AddEvent(EventKind::kToolEnd, static_cast<float>(tool_call->id),
           base::StrCat({tool_call->name, " rejected: ", reason}));
  MarkToolCallChanged(*tool_call);
  RaiseToolIssue("tool_rejected", *tool_call, IssueSeverity::kError,
                 base::StrCat({"Tool call rejected: ", tool_call->name}),
                 base::StrCat({"The tool call never reached the browser: ",
                               reason, ". The model was told it failed."}));
}

void TtcDebugSession::OnToolDispatched(std::string_view call_id,
                                       std::string_view name,
                                       std::string_view arguments_json,
                                       const std::optional<TabSnapshot>& target,
                                       bool verifying) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ended_) {
    return;
  }
  ToolCall* tool_call = FindOrCreateToolCall(call_id, name);
  if (!tool_call) {
    return;
  }
  if (tool_call->arguments_json.empty()) {
    tool_call->arguments_json = Truncate(arguments_json);
  }
  tool_call->status = ToolStatus::kRunning;
  tool_call->dispatched_ms = NowMs();
  // Browser-level tools (e.g. switch_tab) don't act on a tab.
  if (target) {
    tool_call->target_before = TruncateTab(*target);
  }
  tool_call->effect =
      verifying ? ToolEffect::kChecking : ToolEffect::kNotChecked;
  MarkToolCallChanged(*tool_call);

  if (target && !IsInForeground(*target)) {
    RaiseToolIssue(
        kBackgroundTarget, *tool_call, IssueSeverity::kWarning,
        base::StrCat({tool_call->name, " is acting on a background tab"}),
        "The tab TTC is voice-focused on isn't the visible, active "
        "tab, so the action may happen where you can't see it. Check "
        "which tab the voice focus tracker picked.");
  }
}

void TtcDebugSession::OnToolNoTarget(std::string_view call_id,
                                     std::string_view name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ended_) {
    return;
  }
  ToolCall* tool_call = FindOrCreateToolCall(call_id, name);
  if (!tool_call) {
    return;
  }
  tool_call->dispatched_ms = NowMs();
  MarkToolCallChanged(*tool_call);
  RaiseToolIssue(
      "tool_no_target", *tool_call, IssueSeverity::kError,
      base::StrCat({tool_call->name, " had no tab to act on"}),
      "TTC isn't voice-focused on any tab, so the action couldn't run.");
}

void TtcDebugSession::OnToolFinished(
    std::string_view call_id,
    bool ok,
    std::string_view result_code,
    std::string_view message,
    const std::optional<TabSnapshot>& target_after) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ended_) {
    return;
  }
  ToolCall* tool_call = FindOrCreateToolCall(call_id, "");
  if (!tool_call) {
    return;
  }
  tool_call->status = ok ? ToolStatus::kOk : ToolStatus::kError;
  tool_call->finished_ms = NowMs();
  tool_call->result_code = Truncate(result_code);
  tool_call->result_message = Truncate(message);
  if (target_after) {
    tool_call->target_after = TruncateTab(*target_after);
  }
  if (!ok && tool_call->effect == ToolEffect::kChecking) {
    tool_call->effect = ToolEffect::kNotChecked;
  }
  const double latency =
      *tool_call->finished_ms -
      tool_call->dispatched_ms.value_or(tool_call->received_ms);
  AddEvent(EventKind::kToolEnd, static_cast<float>(tool_call->id),
           base::StrCat({tool_call->name, " ", result_code, " in ", Ms(latency),
                         message.empty() ? "" : ": ", message}));
  MarkToolCallChanged(*tool_call);
  ResolveIssue(ToolIssueKey("tool_timeout", tool_call->id));

  if (!ok) {
    RaiseToolIssue(
        "tool_error", *tool_call, IssueSeverity::kError,
        base::StrCat({tool_call->name, " failed: ", result_code}),
        message.empty()
            ? std::string("The action failed in the browser.")
            : base::StrCat({"The action failed in the browser: ", message}));
  } else if (target_after && !IsInForeground(*target_after) &&
             !FindOpenIssue(ToolIssueKey(kBackgroundTarget, tool_call->id))) {
    // Skipped if the dispatch already flagged a background target.
    RaiseToolIssue(
        kBackgroundTarget, *tool_call, IssueSeverity::kWarning,
        base::StrCat({tool_call->name, " finished in a background tab"}),
        "The action reported success, but the tab it acted on isn't "
        "the visible, active tab.");
  }
}

void TtcDebugSession::OnToolEffect(std::string_view call_id,
                                   bool verified,
                                   std::string_view detail) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ended_) {
    return;
  }
  ToolCall* tool_call = FindToolCall(call_id);
  if (!tool_call) {
    return;
  }
  tool_call->effect = verified ? ToolEffect::kVerified : ToolEffect::kNoEffect;
  tool_call->effect_detail = Truncate(detail);
  MarkToolCallChanged(*tool_call);
  if (!verified) {
    RaiseToolIssue(
        "tool_no_effect", *tool_call, IssueSeverity::kError,
        base::StrCat({tool_call->name, " had no visible effect"}),
        base::StrCat({"The expected result wasn't observed: ", detail,
                      ". The model may have been told it succeeded."}));
  }
}

void TtcDebugSession::OnToolResponseSent(std::string_view call_id,
                                         bool sent,
                                         std::string_view response_json) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ended_) {
    return;
  }
  ToolCall* tool_call = FindOrCreateToolCall(call_id, "");
  if (!tool_call) {
    return;
  }
  tool_call->response_sent_ms = NowMs();
  tool_call->response_json = Truncate(response_json);
  tool_call->response_dropped = !sent;
  MarkToolCallChanged(*tool_call);
  if (!sent) {
    RaiseToolIssue("tool_response_not_sent", *tool_call, IssueSeverity::kError,
                   base::StrCat({"Result of ", tool_call->name, " never sent"}),
                   "The tool finished but there was no session to send the "
                   "result on, so the model never learned the outcome.");
    return;
  }
  awaiting_follow_up_ = tool_call->id;
}

// ---------------------------------------------------------------------------
// Other signals

void TtcDebugSession::HandleContextUpdate(const ContextUpdate& update) {
  AddEvent(EventKind::kContextSent, static_cast<float>(update.content_bytes),
           update.url);
  ResolveIssue("page_context_failed");
}

void TtcDebugSession::OnActorJournalEntry(std::string_view event,
                                          char type,
                                          std::string_view details,
                                          bool has_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ended_) {
    return;
  }
  const std::string label =
      Truncate(base::StrCat({event, " [", std::string(1, type), "]",
                             details.empty() ? "" : " ", details}));
  AddEvent(EventKind::kActor, has_error ? 1.0f : 0.0f, label);
  ToolCall* running = RunningToolCall();
  if (running && running->actor_steps.size() < kMaxActorStepsPerTool) {
    const double now = NowMs();
    running->actor_steps.push_back(base::StrCat(
        {"+", Ms(now - running->dispatched_ms.value_or(now)), " ", label}));
    MarkToolCallChanged(*running);
  }
  if (has_error) {
    RaiseIssue(base::StrCat({"actor_error:", event}),
               {.severity = IssueSeverity::kError,
                .category = IssueCategory::kActor,
                .title = base::StrCat({"Actor error in ", event}),
                .detail = base::StrCat(
                    {"The actor framework logged an error: ", details}),
                .evidence = {{"event", std::string(event)},
                             {"details", std::string(details)}},
                .tool_call_id =
                    running ? std::optional<int>(running->id) : std::nullopt});
  }
}

void TtcDebugSession::OnPageContextFailed(std::string_view url,
                                          std::string_view error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ended_) {
    return;
  }
  RaiseIssue(
      "page_context_failed",
      {.severity = IssueSeverity::kWarning,
       .category = IssueCategory::kPageContext,
       .title = "Page context unavailable",
       .detail = base::StrCat({"The page's content couldn't be "
                               "extracted (",
                               error,
                               "), so the model may be working from a "
                               "stale or empty view of the page."}),
       .evidence = {{"url", std::string(url)}, {"error", std::string(error)}}});
}

void TtcDebugSession::OnPageContextBlocked(std::string_view url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ended_) {
    return;
  }
  RaiseIssue(base::StrCat({"page_context_blocked:", url}),
             {.severity = IssueSeverity::kInfo,
              .category = IssueCategory::kPageContext,
              .title = "Model can't see this page",
              .detail = "Page context isn't sent for this URL (unsupported or "
                        "blocked page), so the model doesn't know what's on "
                        "it.",
              .evidence = {{"url", std::string(url)}}});
}

// ---------------------------------------------------------------------------
// Periodic detectors

bool TtcDebugSession::HasUnplayedAudio() const {
  if (!last_audio_received_ms_) {
    return false;
  }
  const double received = *last_audio_received_ms_;
  return (!last_playback_ms_ || *last_playback_ms_ < received) &&
         (!last_interrupted_ms_ || *last_interrupted_ms_ < received);
}

void TtcDebugSession::MaybeStartDetectorTimer() {
  if (ended_ || detector_timer_.IsRunning()) {
    return;
  }
  detector_timer_.Start(FROM_HERE, kDetectorInterval,
                        base::BindRepeating(&TtcDebugSession::RunDetectors,
                                            base::Unretained(this)));
}

void TtcDebugSession::RunDetectors() {
  if (ended_) {
    return;
  }
  const double now = NowMs();
  const auto age = [now](std::optional<double> t) {
    return t ? now - *t : 0.0;
  };

  // Tools that never finish. Resolved by OnToolFinished().
  for (const ToolCall& tool_call : tool_calls_) {
    if (tool_call.status == ToolStatus::kRunning && tool_call.dispatched_ms &&
        age(tool_call.dispatched_ms) >= kToolTimeout.InMillisecondsF()) {
      SetCondition(ToolIssueKey("tool_timeout", tool_call.id), true, [&] {
        return IssueSpec{
            .severity = IssueSeverity::kWarning,
            .category = IssueCategory::kTool,
            .title = base::StrCat({tool_call.name, " is taking too long"}),
            .detail = base::StringPrintf("Dispatched %.1f s ago with no "
                                         "result from the actor yet.",
                                         age(tool_call.dispatched_ms) / 1000),
            .evidence = ToolEvidence(tool_call),
            .tool_call_id = tool_call.id};
      });
    }
  }

  // The model doesn't react to a tool result. Resolved by OnServerActivity().
  if (awaiting_follow_up_) {
    const ToolCall& tool_call = tool_calls_[*awaiting_follow_up_ - 1];
    if (age(tool_call.response_sent_ms) >=
        kModelFollowUpTimeout.InMillisecondsF()) {
      SetCondition(
          ToolIssueKey("model_silent_after_tool", tool_call.id), true, [&] {
            return IssueSpec{
                .severity = IssueSeverity::kWarning,
                .category = IssueCategory::kModel,
                .title =
                    base::StrCat({"Model went quiet after ", tool_call.name}),
                .detail =
                    "The tool result was sent, but the server sent "
                    "nothing afterwards: no speech, transcript, turn "
                    "completion or further tool call.",
                .evidence = ToolEvidence(tool_call),
                .tool_call_id = tool_call.id};
          });
    }
  }

  SetCondition(
      "mic_silent", consecutive_silent_chunks_ >= kMicSilenceIssueChunks, [] {
        return IssueSpec{
            .severity = IssueSeverity::kWarning,
            .category = IssueCategory::kAudio,
            .title = "Microphone is sending silence",
            .detail = base::StringPrintf(
                "The captured audio has been digital silence for over "
                "%.0f s. The mic may be muted, unavailable or blocked "
                "by the OS.",
                (kChunkDuration * kMicSilenceIssueChunks).InSecondsF())};
      });

  SetCondition("mic_muted", capture_muted_, [] {
    return IssueSpec{.severity = IssueSeverity::kWarning,
                     .category = IssueCategory::kAudio,
                     .title = "Microphone is muted",
                     .detail = "The capture device reports that it's muted."};
  });

  SetCondition(
      "playback_stalled",
      HasUnplayedAudio() && age(last_audio_received_ms_) >=
                                kPlaybackStallTimeout.InMillisecondsF(),
      [&] {
        return IssueSpec{
            .severity = IssueSeverity::kError,
            .category = IssueCategory::kAudio,
            .title = "Reply audio isn't playing",
            .detail =
                "Audio arrived from the server but the speaker hasn't "
                "played it.",
            .evidence = {{"audio_chunks_received",
                          base::NumberToString(audio_chunks_received_)},
                         {"audio_chunks_played",
                          base::NumberToString(audio_chunks_played_)}}};
      });

  SetCondition("transport_lost", ever_connected_ && !transport_connected_, [&] {
    return IssueSpec{
        .severity = IssueSeverity::kError,
        .category = IssueCategory::kTransport,
        .title = "Connection to the server lost",
        .detail = "The streaming connection dropped during the session.",
        .evidence = {{"last_server_frame",
                      last_server_frame_ms_
                          ? base::StringPrintf(
                                "%.1f s ago", age(last_server_frame_ms_) / 1000)
                          : std::string("never")}}};
  });
}

// ---------------------------------------------------------------------------
// Export

void TtcDebugSession::AppendProblemsToValue(base::DictValue& dict) const {
  auto tab_value = [](const std::optional<TabSnapshot>& tab) {
    base::DictValue value;
    if (tab) {
      value.Set("tab_id", tab->tab_id);
      value.Set("url", tab->url);
      value.Set("title", tab->title);
      value.Set("visible", tab->visible);
      value.Set("active_in_window", tab->active_in_window);
    }
    return value;
  };

  base::ListValue tool_calls;
  for (const ToolCall& tool_call : tool_calls_) {
    base::ListValue steps;
    for (const std::string& step : tool_call.actor_steps) {
      steps.Append(step);
    }
    base::DictValue value;
    value.Set("id", tool_call.id);
    value.Set("call_id", tool_call.call_id);
    value.Set("name", tool_call.name);
    value.Set("arguments", tool_call.arguments_json);
    value.Set("turn", tool_call.turn_index);
    value.Set("received_ms", tool_call.received_ms);
    value.Set("status", ToolStatusName(tool_call.status));
    value.Set("effect", ToolEffectName(tool_call.effect));
    value.Set("effect_detail", tool_call.effect_detail);
    value.Set("target_before", tab_value(tool_call.target_before));
    value.Set("target_after", tab_value(tool_call.target_after));
    value.Set("result_code", tool_call.result_code);
    value.Set("result_message", tool_call.result_message);
    value.Set("response", tool_call.response_json);
    value.Set("response_dropped", tool_call.response_dropped);
    value.Set("model_followed_up", tool_call.model_followed_up);
    value.Set("actor_steps", std::move(steps));
    if (tool_call.dispatched_ms) {
      value.Set("dispatched_ms", *tool_call.dispatched_ms);
    }
    if (tool_call.finished_ms) {
      value.Set("finished_ms", *tool_call.finished_ms);
    }
    if (tool_call.response_sent_ms) {
      value.Set("response_sent_ms", *tool_call.response_sent_ms);
    }
    tool_calls.Append(std::move(value));
  }
  dict.Set("tool_calls", std::move(tool_calls));

  base::ListValue issues;
  for (const Issue& issue : issues_) {
    base::DictValue evidence;
    for (const auto& [key, value] : issue.evidence) {
      evidence.Set(key, value);
    }
    base::DictValue value;
    value.Set("id", issue.id);
    value.Set("key", issue.key);
    value.Set("severity", IssueSeverityName(issue.severity));
    value.Set("category", IssueCategoryName(issue.category));
    value.Set("title", issue.title);
    value.Set("detail", issue.detail);
    value.Set("evidence", std::move(evidence));
    value.Set("first_ms", issue.first_ms);
    value.Set("last_ms", issue.last_ms);
    value.Set("count", issue.count);
    value.Set("resolved", issue.resolved);
    if (issue.turn_index) {
      value.Set("turn", *issue.turn_index);
    }
    if (issue.tool_call_id) {
      value.Set("tool_call", *issue.tool_call_id);
    }
    issues.Append(std::move(value));
  }
  dict.Set("issues", std::move(issues));
}

}  // namespace ttc
