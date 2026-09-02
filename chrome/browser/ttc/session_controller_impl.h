// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_SESSION_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_TTC_SESSION_CONTROLLER_IMPL_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ttc/session_controller.h"

namespace ttc {

class TtcKeyedService;

class SessionControllerImpl : public SessionController {
 public:
  explicit SessionControllerImpl(TtcKeyedService& service);
  ~SessionControllerImpl() override;
  SessionControllerImpl(const SessionControllerImpl&) = delete;
  SessionControllerImpl& operator=(const SessionControllerImpl&) = delete;

 private:
  // Safe because TtcKeyedService owns this object and outlives it. Gets
  // assigned on construction.
  const raw_ref<TtcKeyedService> service_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_SESSION_CONTROLLER_IMPL_H_
