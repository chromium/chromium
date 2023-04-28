// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_CONTEXT_MANAGER_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_CONTEXT_MANAGER_H_

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/infra/described.h"
#include "components/keyed_service/core/keyed_service.h"

namespace borealis {

class BorealisContext;

class BorealisContextManager : public KeyedService {
 public:
  // An attempt to launch borealis. If the launch succeeds, holds a reference to
  // the context created for that launch, otherwise holds an error.
  using ContextOrFailure =
      base::expected<BorealisContext*, Described<BorealisStartupResult>>;

  // Convenience definition for the callback provided by clients wanting to
  // launch borealis.
  using ResultCallback = base::OnceCallback<void(ContextOrFailure)>;

  BorealisContextManager() = default;
  BorealisContextManager(const BorealisContextManager&) = delete;
  BorealisContextManager& operator=(const BorealisContextManager&) = delete;
  ~BorealisContextManager() override = default;

  // Starts the Borealis VM and/or runs the callback when it is running.
  virtual void StartBorealis(ResultCallback callback) = 0;

  // Returns true if the VM is currently running.
  virtual bool IsRunning() = 0;

  // Stop the current running state, re-initializing the context manager
  // to the state it was in prior to being started. All pending callbacks are
  // invoked with kCancelled result. Invokes |on_shutdown_callback| with the
  // result of the operation when it completes.
  virtual void ShutDownBorealis(
      base::OnceCallback<void(BorealisShutdownResult)> on_shutdown_callback =
          base::DoNothing()) = 0;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_CONTEXT_MANAGER_H_
