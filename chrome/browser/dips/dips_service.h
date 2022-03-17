// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_SERVICE_H_
#define CHROME_BROWSER_DIPS_DIPS_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/dips/dips_storage.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

namespace dips {

class DIPSService : public KeyedService {
 public:
  ~DIPSService() override;

  static DIPSService* Get(content::BrowserContext* context);

  DIPSStorage* storage() { return &storage_; }

 private:
  // So DIPSServiceFactory::BuildServiceInstanceFor can call the constructor.
  friend class DIPSServiceFactory;
  explicit DIPSService(content::BrowserContext* context);

  raw_ptr<content::BrowserContext> browser_context_;
  DIPSStorage storage_;
};

}  // namespace dips

#endif  // CHROME_BROWSER_DIPS_DIPS_SERVICE_H_
