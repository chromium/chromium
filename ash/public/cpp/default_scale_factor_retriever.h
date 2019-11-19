// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_DEFAULT_SCALE_FACTOR_RETRIEVER_H_
#define ASH_PUBLIC_CPP_DEFAULT_SCALE_FACTOR_RETRIEVER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/mojom/cros_display_config.mojom.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// A utility class to retrieve default scale factor from ash shell
// asynchronously. It consists of two steps to minimize the
// latency.
// 1) Start querying by passing a CrosDisplayController.
// 2) Pass callback which will be called when the default
// scale factor is obtained.
class ASH_PUBLIC_EXPORT DefaultScaleFactorRetriever {
 public:
  using GetDefaultScaleFactorCallback = base::OnceCallback<void(float)>;

  DefaultScaleFactorRetriever();
  ~DefaultScaleFactorRetriever();

  // Start the query process.
  void Start(mojo::PendingRemote<mojom::CrosDisplayConfigController>
                 cros_display_config);

  // Get the default scale factor. The scale factor will be passed
  // as an argument to the |callback|. The callback may be call synchronously
  // if the scale factor is already available, or may be called
  // asynchronously if the query is still in progress.
  // This will automatically cancel the pending callback if any.
  void GetDefaultScaleFactor(GetDefaultScaleFactorCallback callback);

  // Cancels pending callback if any.
  void CancelCallback();

  void SetDefaultScaleFactorForTest(float scale_factor);

 private:
  void OnDefaultScaleFactorRetrieved(float scale_factor);

  float default_scale_factor_ = -1.f;
  mojo::Remote<mojom::CrosDisplayConfigController> cros_display_config_;
  GetDefaultScaleFactorCallback callback_;

  // WeakPtrFactory to use callbacks.
  base::WeakPtrFactory<DefaultScaleFactorRetriever> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_DEFAULT_SCALE_FACTOR_RETRIEVER_H_
