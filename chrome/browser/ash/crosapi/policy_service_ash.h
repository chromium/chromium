// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_POLICY_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_POLICY_SERVICE_ASH_H_

#include "chromeos/crosapi/mojom/policy_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

class PolicyServiceAsh : public mojom::PolicyService {
 public:
  PolicyServiceAsh();
  PolicyServiceAsh(const PolicyServiceAsh&) = delete;
  PolicyServiceAsh& operator=(const PolicyServiceAsh&) = delete;
  ~PolicyServiceAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::PolicyService> receiver);

  // crosapi::mojom::PolicyService:
  void ReloadPolicy() override;

 private:
  mojo::ReceiverSet<mojom::PolicyService> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_POLICY_SERVICE_ASH_H_
