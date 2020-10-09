// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_CONTEXT_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_CONTEXT_MANAGER_H_

#include "base/callback.h"
#include "components/keyed_service/core/keyed_service.h"

namespace borealis {

class BorealisContext;

using BorealisContextCallback =
    base::OnceCallback<void(const BorealisContext&)>;

class BorealisContextManager : public KeyedService {
 public:
  BorealisContextManager() = default;
  BorealisContextManager(const BorealisContextManager&) = delete;
  BorealisContextManager& operator=(const BorealisContextManager&) = delete;
  ~BorealisContextManager() override = default;

  // Starts the Borealis VM and/or runs the callback when it is running.
  virtual void StartBorealis(BorealisContextCallback callback) = 0;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_CONTEXT_MANAGER_H_
