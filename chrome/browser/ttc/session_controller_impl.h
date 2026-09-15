// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_SESSION_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_TTC_SESSION_CONTROLLER_IMPL_H_

#include <memory>

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ttc/core/ttc_page_context_monitor.h"
#include "chrome/browser/ttc/session_controller.h"
#include "chrome/browser/ttc/session_view_delegate.h"

namespace ttc {

class Conversation;
class SessionView;
class TtcKeyedService;

class SessionControllerImpl : public SessionController,
                              public SessionViewDelegate {
 public:
  explicit SessionControllerImpl(TtcKeyedService& service);
  ~SessionControllerImpl() override;
  SessionControllerImpl(const SessionControllerImpl&) = delete;
  SessionControllerImpl& operator=(const SessionControllerImpl&) = delete;

  // SessionController implementation:
  void GetPageContext(FetchCompleteCallback callback) override;

  SessionView& session_view() { return CHECK_DEREF(session_view_.get()); }
  Conversation& conversation() { return CHECK_DEREF(conversation_.get()); }

 private:
  // Invoked by `page_context_monitor_` when the monitored page changes.
  void OnPageContextChanged();

  // Safe because TtcKeyedService owns this object and outlives it. Gets
  // assigned on construction.
  const raw_ref<TtcKeyedService> service_;

  // Never null
  std::unique_ptr<Conversation> conversation_;
  std::unique_ptr<SessionView> session_view_;

  std::unique_ptr<TtcPageContextMonitor> page_context_monitor_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_SESSION_CONTROLLER_IMPL_H_
