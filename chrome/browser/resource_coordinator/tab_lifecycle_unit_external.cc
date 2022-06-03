// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/utils.h"

namespace resource_coordinator {

// static
TabLifecycleUnitExternal* TabLifecycleUnitExternal::FromWebContents(
    content::WebContents* web_contents) {
  return GetTabLifecycleUnitSource()->GetTabLifecycleUnitExternal(web_contents);
}

// static
void TabLifecycleUnitExternal::AddTabLifecycleObserver(
    TabLifecycleObserver* observer) {
  GetTabLifecycleUnitSource()->AddTabLifecycleObserver(observer);
}

// static
void TabLifecycleUnitExternal::RemoveTabLifecycleObserver(
    TabLifecycleObserver* observer) {
  GetTabLifecycleUnitSource()->RemoveTabLifecycleObserver(observer);
}

}  // namespace resource_coordinator
