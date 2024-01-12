// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_NONCLOSABLE_APP_TOAST_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_NONCLOSABLE_APP_TOAST_SERVICE_ASH_H_

#include <string>

#include "chromeos/crosapi/mojom/nonclosable_app_toast_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Implements UI for nonclosable web apps.
class NonclosableAppToastServiceAsh : public mojom::NonclosableAppToastService {
 public:
  NonclosableAppToastServiceAsh();
  NonclosableAppToastServiceAsh(const NonclosableAppToastServiceAsh&) = delete;
  NonclosableAppToastServiceAsh& operator=(
      const NonclosableAppToastServiceAsh& other) = delete;
  ~NonclosableAppToastServiceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::NonclosableAppToastService> receiver);

  // mojom::NonclosableAppToastService:
  void OnUserAttemptedClose(const std::string& app_id,
                            const std::string& app_name) override;

 private:
  mojo::ReceiverSet<mojom::NonclosableAppToastService>
      nonclosable_app_toast_service_receiver_set_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_NONCLOSABLE_APP_TOAST_SERVICE_ASH_H_
