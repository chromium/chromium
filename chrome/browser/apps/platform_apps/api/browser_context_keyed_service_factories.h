// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_API_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_API_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_

namespace chrome_apps {
namespace api {

// Ensures the existence of any BrowserContextKeyedServiceFactory provided by
// the Chrome Apps API code.
void EnsureBrowserContextKeyedServiceFactoriesBuilt();

}  // namespace api
}  // namespace chrome_apps

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_API_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_
