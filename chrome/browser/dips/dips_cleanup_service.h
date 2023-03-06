// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_CLEANUP_SERVICE_H_
#define CHROME_BROWSER_DIPS_DIPS_CLEANUP_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

class DIPSCleanupService : public KeyedService {
 public:
  ~DIPSCleanupService() override;

  static DIPSCleanupService* Get(content::BrowserContext* context);

  void WaitOnCleanupForTesting();

 private:
  // So DIPSCleanupServiceFactory::BuildServiceInstanceFor can call the
  // constructor.
  friend class DIPSCleanupServiceFactory;
  explicit DIPSCleanupService(content::BrowserContext* context);

  void OnCleanupFinished();

  base::RunLoop wait_for_cleanup_;
  base::WeakPtrFactory<DIPSCleanupService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DIPS_DIPS_CLEANUP_SERVICE_H_
