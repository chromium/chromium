// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_SYSTEM_DISPLAY_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_SYSTEM_DISPLAY_ASH_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/system_display.mojom.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/common/api/system_display.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// The ash-chrome implementation of the SystemDisplay crosapi interface.
// This class must only be used from the main thread.
class SystemDisplayAsh : public mojom::SystemDisplay {
 public:
  // This type was generated from IDL.
  using DisplayUnitInfo = extensions::api::system_display::DisplayUnitInfo;

  SystemDisplayAsh();
  SystemDisplayAsh(const SystemDisplayAsh&) = delete;
  SystemDisplayAsh& operator=(const SystemDisplayAsh&) = delete;
  ~SystemDisplayAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::SystemDisplay> receiver);

  // crosapi::mojom::SystemDisplay:
  void GetDisplayUnitInfoList(bool single_unified,
                              GetDisplayUnitInfoListCallback callback) override;

 private:
  // Receiver for extensions::DisplayInfoProvider::GetAllDisplaysInfo().
  void OnDisplayInfoResult(GetDisplayUnitInfoListCallback callback,
                           std::vector<DisplayUnitInfo> src_info_list);

  // Support any number of connections.
  mojo::ReceiverSet<mojom::SystemDisplay> receivers_;

  base::WeakPtrFactory<SystemDisplayAsh> weak_ptr_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_SYSTEM_DISPLAY_ASH_H_
