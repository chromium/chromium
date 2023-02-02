// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_TEST_UTILS_H_
#define CHROME_BROWSER_DIPS_DIPS_TEST_UTILS_H_

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/dips/dips_redirect_info.h"
#include "chrome/browser/dips/dips_service.h"
#include "url/gurl.h"

class RedirectChainObserver : public DIPSService::Observer {
 public:
  explicit RedirectChainObserver(DIPSService* service, GURL final_url);
  ~RedirectChainObserver() override;

  void OnChainHandled(const DIPSRedirectChainInfoPtr& chain) override;

  void Wait();

 private:
  GURL final_url_;
  base::RunLoop run_loop_;
  base::ScopedObservation<DIPSService, Observer> obs_{this};
};

#endif  // CHROME_BROWSER_DIPS_DIPS_TEST_UTILS_H_
