// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_LOAD_WAITER_H_
#define CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_LOAD_WAITER_H_

#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"

// Class used to run a message loop waiting for the TabRestoreService to finish
// loading. Does nothing if the TabRestoreService was already loaded.
class TabRestoreServiceLoadWaiter : public sessions::TabRestoreServiceObserver {
 public:
  explicit TabRestoreServiceLoadWaiter(sessions::TabRestoreService* service);
  ~TabRestoreServiceLoadWaiter() override;

  void Wait();

 private:
  // TabRestoreServiceObserver:
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override {}
  void TabRestoreServiceLoaded(sessions::TabRestoreService* service) override;

  sessions::TabRestoreService* const service_;
  base::RunLoop run_loop_;
  ScopedObserver<sessions::TabRestoreService,
                 sessions::TabRestoreServiceObserver>
      observer_{this};
};

#endif  // CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_LOAD_WAITER_H_
