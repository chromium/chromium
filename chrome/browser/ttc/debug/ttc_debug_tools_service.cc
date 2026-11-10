// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/debug/ttc_debug_tools_service.h"

#include <utility>

#include "base/time/default_tick_clock.h"
#include "chrome/browser/ttc/debug/ttc_debug_session.h"
#include "chrome/browser/ttc/debug/ttc_debug_tools_service_factory.h"

namespace ttc {

// static
TtcDebugToolsService* TtcDebugToolsService::Get(
    content::BrowserContext* context) {
  return TtcDebugToolsServiceFactory::GetForBrowserContext(context);
}

TtcDebugToolsService::TtcDebugToolsService()
    : clock_(base::DefaultTickClock::GetInstance()) {}

TtcDebugToolsService::~TtcDebugToolsService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

TtcDebugSession* TtcDebugToolsService::BeginSession(SessionJournal* journal) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (current_session_) {
    current_session_->End();
  }
  current_session_ =
      std::make_unique<TtcDebugSession>(journal, kAudioCapBytes, clock_);
  session_changed_callbacks_.Notify();
  return current_session_.get();
}

void TtcDebugToolsService::EndSession(TtcDebugSession* session) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (session && session == current_session_.get()) {
    session->End();
    session_changed_callbacks_.Notify();
  }
}

base::CallbackListSubscription TtcDebugToolsService::AddSessionChangedCallback(
    base::RepeatingClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return session_changed_callbacks_.Add(std::move(callback));
}

void TtcDebugToolsService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_session_.reset();
}

}  // namespace ttc
