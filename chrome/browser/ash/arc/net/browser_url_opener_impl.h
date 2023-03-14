// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_NET_BROWSER_URL_OPENER_IMPL_H_
#define CHROME_BROWSER_ASH_ARC_NET_BROWSER_URL_OPENER_IMPL_H_

#include "ash/components/arc/net/browser_url_opener.h"

namespace arc {

// Implementation class for ARC network code  to open URLs through Chrome
// browser.
class BrowserUrlOpenerImpl : public BrowserUrlOpener {
 public:
  BrowserUrlOpenerImpl() = default;
  BrowserUrlOpenerImpl(const BrowserUrlOpenerImpl&) = delete;
  BrowserUrlOpenerImpl& operator=(const BrowserUrlOpenerImpl&) = delete;
  ~BrowserUrlOpenerImpl() override = default;

  // Opens the specified `url` in a new browser tab.
  void OpenUrl(GURL url) override;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_NET_BROWSER_URL_OPENER_IMPL_H_
