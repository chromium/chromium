// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOBSTER_LOBSTER_CONTROLLER_H_
#define ASH_LOBSTER_LOBSTER_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "ash/lobster/lobster_entry_point_enums.h"
#include "ash/public/cpp/lobster/lobster_enums.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class LobsterClient;
class LobsterClientFactory;
class LobsterSessionImpl;

class ASH_EXPORT LobsterController {
 public:
  class Trigger {
   public:
    explicit Trigger(LobsterController* controller,
                     std::unique_ptr<LobsterClient> client,
                     LobsterEntryPoint entry_point,
                     LobsterMode mode);
    ~Trigger();

    void Fire(std::optional<std::string> query);

   private:
    enum class State {
      kReady,
      kDisabled,
    };

    // Not owned by this class
    raw_ptr<LobsterController> controller_;

    // The client to use for the session created with this trigger.
    std::unique_ptr<LobsterClient> client_;

    State state_;

    LobsterEntryPoint entry_point_;

    LobsterMode mode_;
  };

  LobsterController();
  ~LobsterController();

  void SetClientFactory(LobsterClientFactory* client_factory);

  std::unique_ptr<Trigger> CreateTrigger(LobsterEntryPoint entry_point,
                                         bool support_image_insertion);

 private:
  friend class Trigger;

  void StartSession(std::unique_ptr<LobsterClient> client,
                    std::optional<std::string> query,
                    LobsterEntryPoint entry_point,
                    LobsterMode mode);

  // Not owned by this class.
  raw_ptr<LobsterClientFactory> client_factory_;

  // Only one session can exist at a time. If a trigger fires while a session
  // is active, the current session is ended and a new one is started.
  std::unique_ptr<LobsterSessionImpl> active_session_;
};

}  // namespace ash

#endif  // ASH_LOBSTER_LOBSTER_CONTROLLER_H_
