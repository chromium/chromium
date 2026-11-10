// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Real-time problem detection for TtcDebugSession.

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ttc/debug/ttc_debug_session.h"

namespace ttc {

TtcDebugSession::Issue::Issue() = default;
TtcDebugSession::Issue::Issue(const Issue&) = default;
TtcDebugSession::Issue& TtcDebugSession::Issue::operator=(const Issue&) =
    default;
TtcDebugSession::Issue::~Issue() = default;

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
  }
}

// ---------------------------------------------------------------------------
// Issues

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
// Other signals

void TtcDebugSession::HandleContextUpdate(const ContextUpdate& update) {
  AddEvent(EventKind::kContextSent, static_cast<float>(update.content_bytes),
           update.url);
  ResolveIssue("page_context_failed");
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
    issues.Append(std::move(value));
  }
  dict.Set("issues", std::move(issues));
}

}  // namespace ttc
