// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_RECEIVER_BOCA_RECEIVER_SERVICE_H_
#define CHROME_BROWSER_ASH_BOCA_RECEIVER_BOCA_RECEIVER_SERVICE_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace gcm {
class GCMDriver;
}  // namespace gcm

namespace instance_id {
class InstanceIDDriver;
}  // namespace instance_id

namespace ash {

namespace boca {
class FCMHandler;
class SpotlightRemotingClientManager;
}  // namespace boca

// Service responsible for managing Boca receiver features.
class BocaReceiverService : public KeyedService {
 public:
  // `gcm_driver` and `instance_id_driver` are supplied by
  // BocaReceiverServiceFactory, which owns the dependency on the //chrome
  // Profile-keyed factories that vend them.
  BocaReceiverService(Profile* profile,
                      gcm::GCMDriver* gcm_driver,
                      instance_id::InstanceIDDriver* instance_id_driver);

  BocaReceiverService(const BocaReceiverService&) = delete;
  BocaReceiverService& operator=(const BocaReceiverService&) = delete;

  ~BocaReceiverService() override;

  // KeyedService:
  void Shutdown() override;

  boca::FCMHandler* fcm_handler() const;

  boca::SpotlightRemotingClientManager* remoting_client() const;

 private:
  std::unique_ptr<boca::FCMHandler> fcm_handler_;
  std::unique_ptr<boca::SpotlightRemotingClientManager> remoting_client_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BOCA_RECEIVER_BOCA_RECEIVER_SERVICE_H_
