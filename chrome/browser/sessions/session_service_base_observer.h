// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_SERVICE_BASE_OBSERVER_H_
#define CHROME_BROWSER_SESSIONS_SESSION_SERVICE_BASE_OBSERVER_H_

#include "base/observer_list_types.h"

class SessionServiceBase;

// Observes the SessionServiceBase objects and notifies clients upon
// destruction.
class SessionServiceBaseObserver : public base::CheckedObserver {
 public:
  SessionServiceBaseObserver() = default;
  ~SessionServiceBaseObserver() override = default;

  // Called when SessionServiceBase is destroyed.
  virtual void OnDestroying(SessionServiceBase* service) = 0;
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_SERVICE_BASE_OBSERVER_H_
