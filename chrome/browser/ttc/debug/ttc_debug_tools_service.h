// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_DEBUG_TTC_DEBUG_TOOLS_SERVICE_H_
#define CHROME_BROWSER_TTC_DEBUG_TTC_DEBUG_TOOLS_SERVICE_H_

#include <stddef.h>

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

namespace base {
class TickClock;
}

namespace ttc {

class SessionJournal;
class TtcDebugSession;

// Holds the debug record of the most recent TTC session, for
// chrome://ttc-internals. Records are kept in memory only: a new TTC session
// replaces the previous record, and everything is dropped on shutdown.
//
// Only created when both ttc::kTtc and ttc::kTtcDebugTools are enabled.
class TtcDebugToolsService : public KeyedService {
 public:
  // Returns the service for `context`, or null if the debug tools are not
  // enabled for it.
  static TtcDebugToolsService* Get(content::BrowserContext* context);

  TtcDebugToolsService();
  TtcDebugToolsService(const TtcDebugToolsService&) = delete;
  TtcDebugToolsService& operator=(const TtcDebugToolsService&) = delete;
  ~TtcDebugToolsService() override;

  base::WeakPtr<TtcDebugToolsService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Upper bound on the audio recorded per TTC session.
  static constexpr size_t kAudioCapBytes = 64 * 1024 * 1024;

  // Starts a debug record for a new TTC session, replacing the previous one.
  // `journal` may be null; if not, it must stay valid until EndSession() is
  // called for the returned session.
  TtcDebugSession* BeginSession(SessionJournal* journal);

  // Marks `session` as ended. Its data stays available until the next
  // BeginSession() or shutdown.
  void EndSession(TtcDebugSession* session);

  // The most recent debug record, if any.
  TtcDebugSession* current_session() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return current_session_.get();
  }

  // Runs `callback` when current_session() is replaced or ends.
  base::CallbackListSubscription AddSessionChangedCallback(
      base::RepeatingClosure callback);

  void SetTickClockForTesting(const base::TickClock* clock) { clock_ = clock; }

  // KeyedService:
  void Shutdown() override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<TtcDebugSession> current_session_;
  base::RepeatingClosureList session_changed_callbacks_;
  raw_ptr<const base::TickClock> clock_;

  base::WeakPtrFactory<TtcDebugToolsService> weak_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_DEBUG_TTC_DEBUG_TOOLS_SERVICE_H_
