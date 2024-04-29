// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/browser_instance/browser_app_instance_forwarder.h"

#include <utility>

#include "base/unguessable_token.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_tracker.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/lacros/window_utility.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "chromeos/crosapi/mojom/browser_app_instance_registry.mojom.h"

namespace apps {

BrowserAppInstanceForwarder::BrowserAppInstanceForwarder(
    BrowserAppInstanceTracker& tracker,
    mojo::Remote<crosapi::mojom::BrowserAppInstanceRegistry>& registry)
    : registry_(registry), tracker_(tracker) {
  tracker_observation_.Observe(&tracker);
  (*registry_)
      ->RegisterController(
          controller_receiver_.BindNewPipeAndPassRemoteWithVersion());
}
BrowserAppInstanceForwarder::~BrowserAppInstanceForwarder() = default;

void BrowserAppInstanceForwarder::OnBrowserWindowAdded(
    const apps::BrowserWindowInstance& instance) {
  (*registry_)->OnBrowserWindowAdded(instance.ToUpdate());
}

void BrowserAppInstanceForwarder::OnBrowserWindowUpdated(
    const apps::BrowserWindowInstance& instance) {
  (*registry_)->OnBrowserWindowUpdated(instance.ToUpdate());
}

void BrowserAppInstanceForwarder::OnBrowserWindowRemoved(
    const apps::BrowserWindowInstance& instance) {
  (*registry_)->OnBrowserWindowRemoved(instance.ToUpdate());
}

void BrowserAppInstanceForwarder::OnBrowserAppAdded(
    const apps::BrowserAppInstance& instance) {
  (*registry_)->OnBrowserAppAdded(instance.ToUpdate());
}

void BrowserAppInstanceForwarder::OnBrowserAppUpdated(
    const apps::BrowserAppInstance& instance) {
  (*registry_)->OnBrowserAppUpdated(instance.ToUpdate());
}

void BrowserAppInstanceForwarder::OnBrowserAppRemoved(
    const apps::BrowserAppInstance& instance) {
  (*registry_)->OnBrowserAppRemoved(instance.ToUpdate());
}

void BrowserAppInstanceForwarder::ActivateTabInstance(
    const base::UnguessableToken& id) {
  tracker_->ActivateTabInstance(id);
}

}  // namespace apps
