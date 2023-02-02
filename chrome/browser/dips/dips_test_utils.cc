// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_test_utils.h"

RedirectChainObserver::RedirectChainObserver(DIPSService* service,
                                             GURL final_url)
    : final_url_(std::move(final_url)) {
  obs_.Observe(service);
}

RedirectChainObserver::~RedirectChainObserver() = default;

void RedirectChainObserver::OnChainHandled(
    const DIPSRedirectChainInfoPtr& chain) {
  if (chain->final_url == final_url_) {
    run_loop_.Quit();
  }
}

void RedirectChainObserver::Wait() {
  run_loop_.Run();
}
