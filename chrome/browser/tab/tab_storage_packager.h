// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STORAGE_PACKAGER_H_
#define CHROME_BROWSER_TAB_TAB_STORAGE_PACKAGER_H_

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/tab/android_tab_package.h"
#include "chrome/browser/tab/tab_storage_package.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

// This class is used to package tab data for use in the background thread.
class TabStoragePackager {
 public:
  TabStoragePackager() = default;
  virtual ~TabStoragePackager() = default;

  TabStoragePackager(const TabStoragePackager&) = delete;
  TabStoragePackager& operator=(const TabStoragePackager&) = delete;

  // Packages the tab's data for later use. After packaging a tab, its data
  // is available via the #ReleasePackage() method.
  virtual void Package(TabInterface* tab) = 0;

  // Allows the unique ownership of the underlying TabStoragePackage to be
  // transferred out of the packager. After this call, the stored package will
  // be null.
  virtual std::unique_ptr<TabStoragePackage> ReleasePackage() = 0;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STORAGE_PACKAGER_H_
