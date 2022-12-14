// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_SYNC_UI_STATE_OBSERVER_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_SYNC_UI_STATE_OBSERVER_H_

#include "base/observer_list_types.h"

class AppSyncUIStateObserver : public base::CheckedObserver {
 public:
  // Invoked when the UI status of app sync has changed.
  virtual void OnAppSyncUIStatusChanged() = 0;

 protected:
  ~AppSyncUIStateObserver() override;
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_SYNC_UI_STATE_OBSERVER_H_
