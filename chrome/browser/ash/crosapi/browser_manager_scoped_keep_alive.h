// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_MANAGER_SCOPED_KEEP_ALIVE_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_MANAGER_SCOPED_KEEP_ALIVE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/crosapi/browser_manager_feature.h"

namespace crosapi {

class BrowserManager;

// Any instance of this class will ensure that the Lacros browser will stay
// running in the background even when no windows are showing.
class BrowserManagerScopedKeepAlive {
 public:
  ~BrowserManagerScopedKeepAlive();

  BrowserManagerScopedKeepAlive(const BrowserManagerScopedKeepAlive&) = delete;
  BrowserManagerScopedKeepAlive& operator=(
      const BrowserManagerScopedKeepAlive&) = delete;

 private:
  friend class BrowserManager;

  // BrowserManager must outlive this instance.
  BrowserManagerScopedKeepAlive(BrowserManager* manager,
                                BrowserManagerFeature feature);

  raw_ptr<BrowserManager> manager_;
  const BrowserManagerFeature feature_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_MANAGER_SCOPED_KEEP_ALIVE_H_
