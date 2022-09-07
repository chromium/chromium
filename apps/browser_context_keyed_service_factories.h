// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_
#define APPS_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_

namespace content {
class BrowserContext;
}

namespace apps {

// Ensures the existence of any BrowserContextKeyedServiceFactory provided by
// the core apps code.
void EnsureBrowserContextKeyedServiceFactoriesBuilt();

// Notifies the relevant BrowserContextKeyedServices for the browser context
// that the application is being terminated.
void NotifyApplicationTerminating(content::BrowserContext* browser_context);

}  // namespace apps

#endif  // APPS_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_
