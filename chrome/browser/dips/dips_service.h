// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_SERVICE_H_
#define CHROME_BROWSER_DIPS_DIPS_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/sequence_bound.h"
#include "chrome/browser/dips/dips_storage.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

namespace content_settings {
class CookieSettings;
}

class DIPSService : public KeyedService {
 public:
  ~DIPSService() override;

  static DIPSService* Get(content::BrowserContext* context);

  base::SequenceBound<DIPSStorage>* storage() { return &storage_; }

  bool ShouldBlockThirdPartyCookies() const;

 private:
  // So DIPSServiceFactory::BuildServiceInstanceFor can call the constructor.
  friend class DIPSServiceFactory;
  explicit DIPSService(content::BrowserContext* context);
  void Shutdown() override;

  scoped_refptr<base::SequencedTaskRunner> CreateTaskRunner();

  raw_ptr<content::BrowserContext> browser_context_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  base::SequenceBound<DIPSStorage> storage_;
};

#endif  // CHROME_BROWSER_DIPS_DIPS_SERVICE_H_
