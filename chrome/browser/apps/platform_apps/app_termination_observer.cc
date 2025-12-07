// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/app_termination_observer.h"

#include "apps/browser_context_keyed_service_factories.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "content/public/browser/browser_context.h"

namespace chrome_apps {

AppTerminationObserver::AppTerminationObserver(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  // base::Unretained(this) is safe here as this object owns |subscription_| and
  // the callback won't be invoked after the subscription is destroyed.
  subscription_ = browser_shutdown::AddAppTerminatingCallback(base::BindOnce(
      &AppTerminationObserver::OnAppTerminating, base::Unretained(this)));
}

AppTerminationObserver::~AppTerminationObserver() = default;

void AppTerminationObserver::Shutdown() {
  // The associated `browser_context_` is shutting down, so it's no longer safe
  // to use (any attempt to access a KeyedService will crash after this point,
  // since the context is marked as dead). Reset the subscription. See
  // https://crbug.com/352003806.
  // See also the note in `OnAppTerminating()`.
  subscription_.reset();
}

void AppTerminationObserver::OnAppTerminating() {
  // NOTE: This fires on application termination, but passes in an associated
  // BrowserContext. If a BrowserContext is actually destroyed *before*
  // application termination, we won't call NotifyApplicationTerminating() for
  // that context. We could instead monitor BrowserContext destruction if this
  // is an issue.
  apps::NotifyApplicationTerminating(browser_context_);
}

}  // namespace chrome_apps
