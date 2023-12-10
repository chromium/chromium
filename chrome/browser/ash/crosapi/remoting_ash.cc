// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/remoting_ash.h"

#include <optional>
#include <utility>

#include "chromeos/crosapi/mojom/remoting.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "remoting/host/chromeos/chromeos_enterprise_params.h"
#include "remoting/host/chromeos/remote_support_host_ash.h"
#include "remoting/host/chromeos/remoting_service.h"

namespace crosapi {

RemotingAsh::RemotingAsh() = default;
RemotingAsh::~RemotingAsh() = default;

void RemotingAsh::BindReceiver(
    mojo::PendingReceiver<mojom::Remoting> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void RemotingAsh::GetSupportHostDetails(
    GetSupportHostDetailsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(remoting::RemoteSupportHostAsh::GetHostDetails());
}

void RemotingAsh::StartSupportSession(
    remoting::mojom::SupportSessionParamsPtr params,
    StartSupportSessionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  remoting::RemotingService::Get().GetSupportHost().StartSession(
      *params.get(), std::nullopt,
      base::BindOnce(
          [](StartSupportSessionCallback callback,
             remoting::mojom::StartSupportSessionResponsePtr response) {
            std::move(callback).Run(std::move(response));
          },
          std::move(callback)));
}

}  // namespace crosapi
