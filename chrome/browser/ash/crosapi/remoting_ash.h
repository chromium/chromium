// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_REMOTING_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_REMOTING_ASH_H_

#include "chromeos/crosapi/mojom/remoting.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// The ash-chrome implementation of the Remoting crosapi interface.  This class
// allows callers to create remote support sessions in ash-chrome from another
// process (e.g. Lacros). This class is affine to the Main/UI thread.
class RemotingAsh : public mojom::Remoting {
 public:
  RemotingAsh();
  RemotingAsh(const RemotingAsh&) = delete;
  RemotingAsh& operator=(const RemotingAsh&) = delete;
  ~RemotingAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::Remoting> receiver);

  // mojom::Remoting implementation.
  void GetSupportHostDetails(GetSupportHostDetailsCallback callback) override;
  void StartSupportSession(remoting::mojom::SupportSessionParamsPtr params,
                           StartSupportSessionCallback callback) override;

 private:
  // Support any number of connections.
  mojo::ReceiverSet<mojom::Remoting> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_REMOTING_ASH_H_
