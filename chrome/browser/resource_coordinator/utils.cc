// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/utils.h"

#include "base/hash/md5.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/resource_coordinator_parts.h"

namespace resource_coordinator {

std::string SerializeOriginIntoDatabaseKey(const url::Origin& origin) {
  return base::MD5String(origin.host());
}

bool URLShouldBeStoredInLocalDatabase(const GURL& url) {
  // Only store information for the HTTP(S) sites for now.
  return url.SchemeIsHTTPOrHTTPS();
}

TabLifecycleUnitSource* GetTabLifecycleUnitSource() {
  DCHECK(g_browser_process);
  auto* source = g_browser_process->resource_coordinator_parts()
                     ->tab_lifecycle_unit_source();
  DCHECK(source);
  return source;
}

}  // namespace resource_coordinator
