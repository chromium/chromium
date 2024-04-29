// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_BROWSER_INSTANCE_BROWSER_APP_INSTANCE_FORWARDER_H_
#define CHROME_BROWSER_APPS_BROWSER_INSTANCE_BROWSER_APP_INSTANCE_FORWARDER_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_observer.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_tracker.h"
#include "chromeos/crosapi/mojom/browser_app_instance_registry.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace apps {

class BrowserAppInstanceTracker;

// Observes the Lacros browser apps tracker and forwards events to Ash. It
// implements the BrowserAppInstanceController crosapi, allowing Ash to perform
// certain operations on instances.
class BrowserAppInstanceForwarder
    : public apps::BrowserAppInstanceObserver,
      public crosapi::mojom::BrowserAppInstanceController {
 public:
  BrowserAppInstanceForwarder(
      BrowserAppInstanceTracker& tracker,
      mojo::Remote<crosapi::mojom::BrowserAppInstanceRegistry>& registry);
  ~BrowserAppInstanceForwarder() override;

  BrowserAppInstanceForwarder(const BrowserAppInstanceForwarder&) = delete;
  BrowserAppInstanceForwarder(BrowserAppInstanceForwarder&&) = delete;
  BrowserAppInstanceForwarder& operator=(const BrowserAppInstanceForwarder&) =
      delete;
  BrowserAppInstanceForwarder& operator=(BrowserAppInstanceForwarder&&) =
      delete;

 private:
  void OnBrowserWindowAdded(
      const apps::BrowserWindowInstance& instance) override;
  void OnBrowserWindowUpdated(
      const apps::BrowserWindowInstance& instance) override;
  void OnBrowserWindowRemoved(
      const apps::BrowserWindowInstance& instance) override;
  void OnBrowserAppAdded(const apps::BrowserAppInstance& instance) override;
  void OnBrowserAppUpdated(const apps::BrowserAppInstance& instance) override;
  void OnBrowserAppRemoved(const apps::BrowserAppInstance& instance) override;

  // crosapi::mojom::BrowserAppInstanceController overrides.
  void ActivateTabInstance(const base::UnguessableToken& instance_id) override;

 private:
  const raw_ref<mojo::Remote<crosapi::mojom::BrowserAppInstanceRegistry>>
      registry_;

  const raw_ref<BrowserAppInstanceTracker> tracker_;

  base::ScopedObservation<BrowserAppInstanceTracker, BrowserAppInstanceObserver>
      tracker_observation_{this};

  mojo::Receiver<crosapi::mojom::BrowserAppInstanceController>
      controller_receiver_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_BROWSER_INSTANCE_BROWSER_APP_INSTANCE_FORWARDER_H_
