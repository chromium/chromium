// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_CONTEXT_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_CONTEXT_MANAGER_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/chromeos/borealis/borealis_metrics.h"
#include "components/keyed_service/core/keyed_service.h"

namespace borealis {

class BorealisContext;

class BorealisContextManager : public KeyedService {
 public:
  // An attempt to launch borealis. If the launch succeeds, holds a reference to
  // the context created for that launch, otherwise holds an error.
  class Result {
   public:
    // Used to indicate that the result was a success.
    explicit Result(const BorealisContext* ctx);

    // Used to indicate the the result was a failure.
    Result(BorealisStartupResult result, std::string failure_reason);

    ~Result();

    // Returns true if the result was successful.
    bool Ok() const;

    // In the event of a failed launch, returns the result code for that error.
    BorealisStartupResult Failure() const;

    // In the event of a failed launch, returns the message provided.
    const std::string& FailureReason() const;

    // In the event of a successful launch, returns a handle to the context
    // created for that launch.
    const BorealisContext& Success() const;

   private:
    BorealisStartupResult result_;
    std::string failure_reason_;
    const BorealisContext* ctx_;
  };

  // Convenience definition for the callback provided by clients wanting to
  // launch borealis.
  using ResultCallback = base::OnceCallback<void(Result)>;

  BorealisContextManager() = default;
  BorealisContextManager(const BorealisContextManager&) = delete;
  BorealisContextManager& operator=(const BorealisContextManager&) = delete;
  ~BorealisContextManager() override = default;

  // Starts the Borealis VM and/or runs the callback when it is running.
  virtual void StartBorealis(ResultCallback callback) = 0;

  // Stop the current running state, re-initializing the context manager
  // to the state it was in prior to being started. All pending callbacks are
  // invoked with kCancelled result.
  virtual void ShutDownBorealis() = 0;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_CONTEXT_MANAGER_H_
