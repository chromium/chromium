// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_BENCHMARKING_H_
#define CHROME_BROWSER_NET_BENCHMARKING_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/net_benchmarking.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace predictors {
class LoadingPredictor;
}

// This class handles Chrome-specific benchmarking IPC messages for the renderer
// process.
// All methods of this class should be called on the IO thread unless the
// contrary is explicitly specified.
class NetBenchmarking : public chrome::mojom::NetBenchmarking {
 public:
  NetBenchmarking(base::WeakPtr<predictors::LoadingPredictor> loading_predictor,
                  int render_process_id);
  ~NetBenchmarking() override;

  // Creates a NetBenchmarking instance and connects it strongly to a mojo pipe.
  // Callers should prefer this over using the constructor directly.
  static void Create(
      base::WeakPtr<predictors::LoadingPredictor> loading_predictor,
      int render_process_id,
      mojo::PendingReceiver<chrome::mojom::NetBenchmarking> receiver);

  // This method is thread-safe.
  static bool CheckBenchmarkingEnabled();

 private:
  // chrome:mojom:NetBenchmarking.
  void CloseCurrentConnections(
      CloseCurrentConnectionsCallback callback) override;
  void ClearCache(ClearCacheCallback callback) override;
  void ClearHostResolverCache(ClearHostResolverCacheCallback callback) override;
  void ClearPredictorCache(ClearPredictorCacheCallback callback) override;

  // These weak pointers should be dereferenced only on the UI thread.
  base::WeakPtr<predictors::LoadingPredictor> loading_predictor_;
  const int render_process_id_;

  DISALLOW_COPY_AND_ASSIGN(NetBenchmarking);
};

#endif  // CHROME_BROWSER_NET_BENCHMARKING_H_
