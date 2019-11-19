// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_EXAMPLE_SESSION_CONTROLLER_CLIENT_H_
#define ASH_SHELL_EXAMPLE_SESSION_CONTROLLER_CLIENT_H_

#include <utility>

#include "ash/session/test_session_controller_client.h"
#include "base/callback.h"
#include "base/macros.h"

namespace ash {

class SessionControllerImpl;

namespace shell {

class ExampleSessionControllerClient : public TestSessionControllerClient {
 public:
  ExampleSessionControllerClient(SessionControllerImpl* controller,
                                 TestPrefServiceProvider* prefs_provider);
  ~ExampleSessionControllerClient() override;

  static ExampleSessionControllerClient* Get();

  void Initialize();

  void set_quit_closure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

  // TestSessionControllerClient
  void RequestLockScreen() override;
  void UnlockScreen() override;
  void RequestSignOut() override;

 private:
  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(ExampleSessionControllerClient);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_EXAMPLE_SESSION_CONTROLLER_CLIENT_H_
