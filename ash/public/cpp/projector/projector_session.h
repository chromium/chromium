// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_SESSION_H_
#define ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_SESSION_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"

namespace ash {

// A checked observer which receives notification of changes to the
// |ProjectorSession|.
class ASH_PUBLIC_EXPORT ProjectorSessionObserver
    : public base::CheckedObserver {
 public:
  // Invoked when the session active state is changed.
  virtual void OnProjectorSessionActiveStateChanged(bool active) {}
};

// Interface to maintain projector session in ash.
class ASH_PUBLIC_EXPORT ProjectorSession {
 public:
  ProjectorSession();
  ProjectorSession(const ProjectorSession&) = delete;
  ProjectorSession& operator=(const ProjectorSession&) = delete;
  virtual ~ProjectorSession();

  static ProjectorSession* Get();

  bool is_active() const { return active_; }

  // Adds/removes the specified |observer|.
  virtual void AddObserver(ProjectorSessionObserver* observer) = 0;
  virtual void RemoveObserver(ProjectorSessionObserver* observer) = 0;

 protected:
  // Keep track of the session active state. Only one active session is allowed.
  bool active_ = false;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_SESSION_H_
