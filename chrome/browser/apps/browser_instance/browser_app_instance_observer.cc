// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/browser_instance/browser_app_instance_observer.h"

namespace apps {

BrowserAppInstanceObserver::~BrowserAppInstanceObserver() = default;

void BrowserAppInstanceObserver::OnBrowserWindowAdded(
    const BrowserWindowInstance& instance) {}
void BrowserAppInstanceObserver::OnBrowserWindowUpdated(
    const BrowserWindowInstance& instance) {}
void BrowserAppInstanceObserver::OnBrowserWindowRemoved(
    const BrowserWindowInstance& instance) {}

void BrowserAppInstanceObserver::OnBrowserAppAdded(
    const BrowserAppInstance& instance) {}
void BrowserAppInstanceObserver::OnBrowserAppUpdated(
    const BrowserAppInstance& instance) {}
void BrowserAppInstanceObserver::OnBrowserAppRemoved(
    const BrowserAppInstance& instance) {}

}  // namespace apps
