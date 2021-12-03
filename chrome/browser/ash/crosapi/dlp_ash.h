// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_DLP_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_DLP_ASH_H_

#include "chromeos/crosapi/mojom/dlp.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Implements the crosapi DLP (Data Leak Prevention) interface. Lives in
// ash-chrome on the UI thread.
class DlpAsh : public mojom::Dlp {
 public:
  DlpAsh();
  DlpAsh(const DlpAsh&) = delete;
  DlpAsh& operator=(const DlpAsh&) = delete;
  ~DlpAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::Dlp> receiver);

  // crosapi::mojom::Dlp:
  void DlpRestrictionsUpdated(
      const std::string& window_id,
      mojom::DlpRestrictionSetPtr restrictions) override;

 private:
  mojo::ReceiverSet<mojom::Dlp> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_DLP_ASH_H_
