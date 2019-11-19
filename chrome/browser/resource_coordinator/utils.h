// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_UTILS_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_UTILS_H_

#include <string>

#include "base/process/process_handle.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace resource_coordinator {

class TabLifecycleUnitSource;

// Serialize an Origin into the representation used by the different databases
// that need it.
std::string SerializeOriginIntoDatabaseKey(const url::Origin& origin);

// Indicates if |url| should have an entry in the local site characteristics
// database.
bool URLShouldBeStoredInLocalDatabase(const GURL& url);

// Get the private memory footprint (in KB) for the process.
int GetPrivateMemoryKB(base::ProcessHandle handle);

// Returns the TabLifecycleUnitSource indirectly owned by g_browser_process.
TabLifecycleUnitSource* GetTabLifecycleUnitSource();

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_UTILS_H_
