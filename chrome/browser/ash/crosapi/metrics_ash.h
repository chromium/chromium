// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_METRICS_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_METRICS_ASH_H_

#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/metrics.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// The ash-chrome implementation of the Metrics crosapi interface.
class MetricsAsh : public mojom::Metrics {
 public:
  MetricsAsh();
  MetricsAsh(const MetricsAsh&) = delete;
  MetricsAsh& operator=(const MetricsAsh&) = delete;
  ~MetricsAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::Metrics> receiver);

  // crosapi::mojom::Metrics:
  void GetFullHardwareClass(GetFullHardwareClassCallback callback) override;

 private:
  // Called when machine statistics are loaded. Can be synchronously invoked if
  // machine statistics are already loaded.
  void OnMachineStatisticsLoaded();

  // A cached copy of the full hardware class.
  std::optional<std::string> full_hardware_class_;

  // Callbacks waiting for full_hardware_class_.
  std::vector<GetFullHardwareClassCallback> callbacks_;

  // This class supports any number of connections.
  mojo::ReceiverSet<mojom::Metrics> receivers_;

  base::WeakPtrFactory<MetricsAsh> weak_ptr_factory_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_METRICS_ASH_H_
