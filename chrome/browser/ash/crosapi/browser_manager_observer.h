// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_MANAGER_OBSERVER_H_

#include "base/observer_list_types.h"
#include "base/version.h"

namespace crosapi {

class BrowserManagerObserver : public base::CheckedObserver {
 public:
  // Invoked when the Mojo connection to lacros-chrome is disconnected.
  virtual void OnMojoDisconnected() {}
  // Invoked when lacros-chrome state changes, without specifying the state.
  virtual void OnStateChanged() {}
  // Invoked when the browser loader has finished loading an image.
  virtual void OnLoadComplete(bool success, const base::Version& version) {}
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_MANAGER_OBSERVER_H_
