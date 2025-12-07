// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_KEYED_SERVICE_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_
#define CHROME_BROWSER_ASH_KEYED_SERVICE_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_

namespace ash {

// Ensures the existence of any BrowserContextKeyedServiceFactory provided by
// the Chrome OS code.
void EnsureBrowserContextKeyedServiceFactoriesBuilt();

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_KEYED_SERVICE_BROWSER_CONTEXT_KEYED_SERVICE_FACTORIES_H_
