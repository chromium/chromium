// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/session/session_observer.h"

#include "ash/public/cpp/session/session_controller.h"

namespace ash {

ScopedSessionObserver::ScopedSessionObserver(SessionObserver* observer)
    : observer_(observer) {
  DCHECK(SessionController::Get());
  SessionController::Get()->AddObserver(observer_);
}

ScopedSessionObserver::~ScopedSessionObserver() {
  DCHECK(SessionController::Get());
  SessionController::Get()->RemoveObserver(observer_);
}

}  // namespace ash
