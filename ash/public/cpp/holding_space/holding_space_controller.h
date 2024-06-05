// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CONTROLLER_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CONTROLLER_H_

#include <map>
#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/account_id/account_id.h"

namespace ash {

class HoldingSpaceClient;
class HoldingSpaceControllerObserver;
class HoldingSpaceModel;

// Keeps track of all registered holding space models per user account and makes
// sure the current active model belongs to the current active user.
// There is expected to exist at most one instance of this class at a time. In
// production the instance is owned by ash::Shell. The instance can be retrieved
// using HoldingSpaceController::Get().
class ASH_PUBLIC_EXPORT HoldingSpaceController : public SessionObserver {
 public:
  HoldingSpaceController();
  HoldingSpaceController(const HoldingSpaceController&) = delete;
  HoldingSpaceController& operator=(const HoldingSpaceController&) = delete;
  ~HoldingSpaceController() override;

  // Returns the global HoldingSpaceController instance. It's set in the
  // HoldingSpaceController constructor, and reset in the destructor. The
  // instance is owned by ash shell.
  static HoldingSpaceController* Get();

  void AddObserver(HoldingSpaceControllerObserver* observer);
  void RemoveObserver(HoldingSpaceControllerObserver* observer);

  // Adds a client and model to it's corresponding user account id in a map.
  void RegisterClientAndModelForUser(const AccountId& account_id,
                                     HoldingSpaceClient* client,
                                     HoldingSpaceModel* model);

  HoldingSpaceClient* client() { return client_; }
  HoldingSpaceModel* model() { return model_; }

 private:
  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  void SetClient(HoldingSpaceClient* client);
  void SetModel(HoldingSpaceModel* model);

  // The currently active holding space client, set by `SetClient()`.
  raw_ptr<HoldingSpaceClient> client_ = nullptr;

  // The currently active holding space model, set by `SetModel()`.
  raw_ptr<HoldingSpaceModel> model_ = nullptr;

  // The currently active user account id.
  AccountId active_user_account_id_;

  using ClientAndModel = std::pair<HoldingSpaceClient*, HoldingSpaceModel*>;
  std::map<const AccountId, ClientAndModel> clients_and_models_by_account_id_;

  base::ObserverList<HoldingSpaceControllerObserver> observers_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_CONTROLLER_H_
