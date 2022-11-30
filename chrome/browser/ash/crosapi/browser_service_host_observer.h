// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_SERVICE_HOST_OBSERVER_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_SERVICE_HOST_OBSERVER_H_

#include "base/observer_list_types.h"
#include "chrome/browser/ash/crosapi/crosapi_id.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {
namespace mojom {
class BrowserService;
}  // namespace mojom

// Interface to observe BrowserService registration from Crosapi clients.
class BrowserServiceHostObserver : public base::CheckedObserver {
 public:
  // Called when a new BrowserService tied to the CrosapiId gets ready.
  // Note that, CrosapiId will be useful to identify where this BrowserService
  // comes from.
  virtual void OnBrowserServiceConnected(CrosapiId id,
                                         mojo::RemoteSetElementId mojo_id,
                                         mojom::BrowserService* browser_service,
                                         uint32_t browser_service_version) {}

  // Called when the BrowserService represented by IDs is disconnected.
  // When this is called, mojom::BrowserService is already destroyed.
  virtual void OnBrowserServiceDisconnected(CrosapiId id,
                                            mojo::RemoteSetElementId mojo_id) {}

  // Called when BrowserServiceHost::RequestRelaunch is called from
  // the Crosapi client.
  virtual void OnBrowserRelaunchRequested(CrosapiId id) {}
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_SERVICE_HOST_OBSERVER_H_
