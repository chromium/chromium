// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_
#define CHROME_BROWSER_CHROMEOS_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_

namespace chromeos {

// Ensures the existence of any BrowserContextKeyedServiceFactory provided by
// the Chrome OS code.
void EnsureBrowserContextKeyedServiceFactoriesBuilt();

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_
